#pragma once

#include "osci_AudioBackgroundThread.h"
#include "osci_AudioBackgroundThreadManager.h"

namespace osci {

AudioBackgroundThread::AudioBackgroundThread(const juce::String& name, AudioBackgroundThreadManager& manager) : juce::Thread(name), manager(manager) {
    manager.registerThread(this);
}

AudioBackgroundThread::~AudioBackgroundThread() {
    deleting = true;
    setShouldBeRunning(false);
    manager.unregisterThread(this);
}

void AudioBackgroundThread::prepare(double sampleRate, int samplesPerBlock) {
    bool threadShouldBeRunning = shouldBeRunning;
    setShouldBeRunning(false);
    
    isPrepared = false;
    samplesPerBlock = samplesPerBlock > 0 ? samplesPerBlock : manager.samplesPerBlock;
    samplesPerTask = juce::jmax(1, prepareTask(sampleRate, samplesPerBlock));
    taskIntervalMs = 1000.0 * samplesPerTask / sampleRate;
    // Keep a complete callback's worth of audio, rounded up to whole tasks.
    const int tasksPerBatch = juce::jmax(1, (samplesPerBlock + samplesPerTask - 1) / samplesPerTask);
    consumer = std::make_unique<BufferConsumer>(tasksPerBatch * samplesPerTask);
    isPrepared = true;
    
    setShouldBeRunning(threadShouldBeRunning);
}

void AudioBackgroundThread::setShouldBeRunning(bool shouldBeRunning, std::function<void()> stopCallback) {
    if (!isPrepared && shouldBeRunning) {
        prepare(manager.sampleRate, manager.samplesPerBlock);
    }
    
    this->shouldBeRunning = shouldBeRunning;
    
    if (!shouldBeRunning && isThreadRunning()) {
        if (stopCallback) {
            stopCallback();
        }
        stop();
    } else if (isPrepared && shouldBeRunning && !isThreadRunning()) {
        start();
    }
}

void AudioBackgroundThread::write(juce::AudioBuffer<float>& buffer) {
    if (isPrepared && isThreadRunning()) {
        for (int i = 0; i < buffer.getNumSamples(); i++) {
            consumer->write(Point::fromAudioBuffer(buffer, i));
        }
    }
}

void AudioBackgroundThread::run() {
    // Split each audio batch into task-sized views without copying samples. For live
    // batches containing multiple tasks, use interruptible waits to spread updates
    // over time instead of displaying a burst whenever a large audio callback arrives.
    // Skip stale slices if we fall behind. Recording instead processes every slice
    // without pacing, relying on queue backpressure to prevent sample loss.
    // Stopping or changing mode discards the remaining slices and resets the timing.
    double nextFrameTime = 0.0;
    auto pacingRevision = taskRevision.load();
    while (!threadShouldExit() && shouldBeRunning) {
        auto* received = consumer->waitUntilFull();
        const auto revision = taskRevision.load();
        if (pacingRevision != revision) {
            nextFrameTime = 0.0;
            pacingRevision = revision;
        }
        // A mode switch may have retired this buffer while the wait was completing.
        if (received == nullptr || received != &consumer->getBuffer()) {
            continue;
        }

        auto& batch = *received;
        const bool paced = !consumer->isBlockingOnWrite() && batch.getNumSamples() > samplesPerTask;
        if (!paced) {
            nextFrameTime = 0.0;
        }
        for (int offset = 0; offset < batch.getNumSamples(); offset += samplesPerTask) {
            if (threadShouldExit() || !shouldBeRunning || revision != taskRevision) {
                break;
            }
            if (paced) {
                offset = paceLiveTask(offset, batch.getNumSamples(), nextFrameTime, revision);
            }
            if (threadShouldExit() || !shouldBeRunning || revision != taskRevision) {
                break;
            }
            // A non-owning view: the consumer retains this batch until all slices are finished.
            juce::AudioBuffer<float> frame(batch.getArrayOfWritePointers(), batch.getNumChannels(), offset,
                                           juce::jmin(samplesPerTask, batch.getNumSamples() - offset));
            runTask(frame);
        }
        if (revision != taskRevision) {
            nextFrameTime = 0.0;
        }
    }
}

int AudioBackgroundThread::paceLiveTask(int offset, int batchSamples, double& nextFrameTime, unsigned revision) {
    auto now = juce::Time::getMillisecondCounterHiRes();
    if (nextFrameTime == 0.0) {
        // One frame of startup headroom for non-aligned batch/callback boundaries.
        nextFrameTime = now + taskIntervalMs;
    }
    const int skippableFrames = (batchSamples - offset - 1) / samplesPerTask;
    const int skipped = static_cast<int>(juce::jlimit(0.0, static_cast<double>(skippableFrames), (now - nextFrameTime) / taskIntervalMs));
    offset += skipped * samplesPerTask;
    nextFrameTime += skipped * taskIntervalMs;
    if (now - nextFrameTime >= taskIntervalMs) {
        nextFrameTime = now; // Recover from a stall without replaying a backlog.
    }
    while (now < nextFrameTime && !threadShouldExit() && shouldBeRunning && revision == taskRevision) {
        wait(nextFrameTime - now);
        now = juce::Time::getMillisecondCounterHiRes();
    }
    nextFrameTime += taskIntervalMs;
    return offset;
}

void AudioBackgroundThread::setBlockOnAudioThread(bool block) {
    if (consumer != nullptr) {
        consumer->setBlockOnWrite(block);
        ++taskRevision;
        notify();
    }
}

void AudioBackgroundThread::start() {
    startThread();
}

void AudioBackgroundThread::stop() {
    ++taskRevision;
    notify();
    if (!deleting) {
        stopTask();
    }
    consumer->forceNotify();
    stopThread(1000);
    // Release recording backpressure only after the queue's reader has stopped.
    if (consumer->isBlockingOnWrite()) {
        consumer->setBlockOnWrite(false);
    }
}

} // namespace osci
