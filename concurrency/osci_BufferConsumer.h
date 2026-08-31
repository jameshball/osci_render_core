#pragma once

#include "../shape/osci_Point.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

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


class BufferConsumer {
public:
    BufferConsumer(std::size_t size) {
        returnBuffer.setSize(6, static_cast<int>(size));
        for (auto& buffer : liveBuffers) {
            buffer.setSize(6, static_cast<int>(size));
        }
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
            for (int i = 0; i < returnBuffer.getNumSamples() && blockOnWrite && !wakeRequested.exchange(false); i++) {
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
            for (;;) {
                if (!sema.acquire() || blockOnWrite || wakeRequested.exchange(false)) {
                    return;
                }
                if ((pendingBuffer.load(std::memory_order_acquire) & newData) != 0) {
                    // Release our previous read buffer and acquire the latest completed one.
                    readBuffer = pendingBuffer.exchange(readBuffer, std::memory_order_acq_rel) & indexMask;
                    return;
                }
            }
        }
    }
    
    // to be used when the audio thread is being destroyed to
    // make sure that everything waiting on it stops waiting.
    void forceNotify() {
        wakeRequested = true;
        sema.release();
        queue->try_enqueue(osci::Point());
    }

    void write(osci::Point point) {
        if (blockOnWrite) {
            queue->wait_enqueue(point);
        } else {
            if (offset >= liveBuffers[writeBuffer].getNumSamples()) {
                // Only the pending buffer may be replaced; the consumer owns its read buffer.
                const auto previous = pendingBuffer.exchange(writeBuffer | newData, std::memory_order_acq_rel);
                writeBuffer = previous & indexMask;
                offset = 0;
                if ((previous & newData) == 0) {
                    sema.release();
                }
            }

            auto writePointers = liveBuffers[writeBuffer].getArrayOfWritePointers();

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
            return liveBuffers[readBuffer];
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
    std::array<juce::AudioBuffer<float>, 3> liveBuffers;
    static constexpr unsigned newData = 4;
    static constexpr unsigned indexMask = 3;
    // Each thread owns its index; only the pending slot transfers buffer ownership.
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    unsigned writeBuffer = 0;
    unsigned readBuffer = 2;
    std::atomic<unsigned> pendingBuffer { 1 };
    std::atomic<bool> wakeRequested { false };
    std::atomic<bool> blockOnWrite = false;
    Semaphore sema{0};
    int offset = 0;
};

} // namespace osci
