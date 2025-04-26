#pragma once

#include <JuceHeader.h>
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
    int requestedDataSize = prepareTask(sampleRate, samplesPerBlock);
    consumer = std::make_unique<BufferConsumer>(requestedDataSize);
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

void AudioBackgroundThread::write(const osci::Point& point) {
    if (isPrepared && isThreadRunning()) {
        consumer->write(point);
    }
}

void AudioBackgroundThread::run() {
    while (!threadShouldExit() && shouldBeRunning) {
        if (consumer->waitUntilFull()) {
            if (shouldBeRunning) {
                runTask(consumer->getBuffer());
            }
        }
    }
}

void AudioBackgroundThread::setBlockOnAudioThread(bool block) {
    consumer->setBlockOnWrite(block);
}

void AudioBackgroundThread::start() {
    startThread();
}

void AudioBackgroundThread::stop() {
    if (!deleting) {
        stopTask();
    }
    consumer->forceNotify();
    stopThread(1000);
}

} // namespace osci
