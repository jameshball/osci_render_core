#pragma once

#include <JuceHeader.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "atomicops.h"
#include "readerwritercircularbuffer.h"

namespace osci {

class Semaphore {
public:
    explicit Semaphore(std::ptrdiff_t initialCount = 1) : semaphore(static_cast<moodycamel::spsc_sema::LightweightSemaphore::ssize_t>(initialCount)) { }

    bool acquire(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        const auto timeoutUsecs = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
        return semaphore.wait(static_cast<std::int64_t>(timeoutUsecs));
    }

    void release() {
        semaphore.signal();
    }

    std::size_t available() const {
        return semaphore.availableApprox();
    }

private:
    moodycamel::spsc_sema::LightweightSemaphore semaphore;
};


// TODO: I am aware this will cause read/write data races, but I don't think
// this matters too much in practice.
class BufferConsumer {
public:
    BufferConsumer(std::size_t size) {
        returnBuffer.setSize(6, static_cast<int>(size));
        buffer1.setSize(6, static_cast<int>(size));
        buffer2.setSize(6, static_cast<int>(size));
        queue = std::make_unique<moodycamel::BlockingReaderWriterCircularBuffer<osci::Point>>(2 * size);
    }

    ~BufferConsumer() {}
    
    // CONSUMER
    // loop dequeuing until full
    // get buffer
    
    // PRODUCER
    // enqueue point
    
    void waitUntilFull() {
        if (blockOnWrite) {
            for (int i = 0; i < returnBuffer.getNumSamples() && blockOnWrite; i++) {
                auto writePointers = returnBuffer.getArrayOfWritePointers();
                osci::Point p;
                queue->wait_dequeue(p);
                writePointers[0][i] = p.x;
                writePointers[1][i] = p.y;
                writePointers[2][i] = p.z;
                writePointers[3][i] = p.r;
                writePointers[4][i] = p.g;
                writePointers[5][i] = p.b;
            }
        } else {
            sema.acquire();
        }
    }
    
    // to be used when the audio thread is being destroyed to
    // make sure that everything waiting on it stops waiting.
    void forceNotify() {
        sema.release();
        queue->try_enqueue(osci::Point());
    }

    void write(osci::Point point) {
        if (blockOnWrite) {
            queue->wait_enqueue(point);
        } else {
            if (offset >= buffer->getNumSamples()) {
                {
                    juce::SpinLock::ScopedLockType scope(bufferLock);
                    buffer = buffer == &buffer1 ? &buffer2 : &buffer1;
                }
                offset = 0;
                sema.release();
            }

            auto writePointers = buffer->getArrayOfWritePointers();

            writePointers[0][offset] = point.x;
            writePointers[1][offset] = point.y;
            writePointers[2][offset] = point.z;
            writePointers[3][offset] = point.r;
            writePointers[4][offset] = point.g;
            writePointers[5][offset] = point.b;
            offset++;
        }
    }

    juce::AudioBuffer<float>& getBuffer() {
        if (blockOnWrite) {
            return returnBuffer;
        } else {
            // whatever buffer is not currently being written to
            juce::SpinLock::ScopedLockType scope(bufferLock);
            return buffer == &buffer1 ? buffer2 : buffer1;
        }
    }
    
    void setBlockOnWrite(bool block) {
        blockOnWrite = block;
        if (blockOnWrite) {
            // Drain any points left in the queue from a previous blocking
            // session. Between recordings, producer/consumer both take the
            // non-blocking path and never touch `queue`, so whatever was in
            // flight when the last recording ended stays there. Without this
            // drain, the first `waitUntilFull()` of the new blocking session
            // would fill `returnBuffer` from those stale points, producing a
            // one-frame "flash back" to the end of the previous recording.
            osci::Point discarded;
            while (queue->try_dequeue(discarded)) {}
            sema.release();
        } else {
            osci::Point item;
            // We dequeue an item so that the audio thread is unblocked
            // if it's trying to wait until the queue is no longer full.
            queue->try_dequeue(item);
            // We enqueue an item so that the consumer is unblocked
            // if it's trying to wait until the queue is no longer empty.
            queue->try_enqueue(item);
        }
    }

private:
    std::unique_ptr<moodycamel::BlockingReaderWriterCircularBuffer<osci::Point>> queue;
    juce::AudioBuffer<float> returnBuffer;
    juce::AudioBuffer<float> buffer1;
    juce::AudioBuffer<float> buffer2;
    juce::SpinLock bufferLock;
    std::atomic<bool> blockOnWrite = false;
    juce::AudioBuffer<float>* buffer = &buffer1;
    Semaphore sema{0};
    int offset = 0;
};

} // namespace osci
