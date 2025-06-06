#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <condition_variable>
#include "readerwritercircularbuffer.h"

namespace osci {

// FROM https://gist.github.com/Kuxe/6bdd5b748b5f11b303a5cccbb8c8a731
/** General semaphore with N permissions **/
class Semaphore {
    const size_t num_permissions;
    size_t avail;
    std::mutex m;
    std::condition_variable cv;
public:
    /** Default constructor. Default semaphore is a binary semaphore **/
    explicit Semaphore(const size_t& num_permissions = 1) : num_permissions(num_permissions), avail(num_permissions) { }

    /** Copy constructor. Does not copy state of mutex or condition variable,
        only the number of permissions and number of available permissions **/
    Semaphore(const Semaphore& s) : num_permissions(s.num_permissions), avail(s.avail) { }

    bool acquire(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock<std::mutex> lk(m);
        bool result = cv.wait_for(lk, timeout, [this] { return avail > 0; });
        if (result) {
            avail--;
        }
        lk.unlock();
        return result;
    }

    void release() {
        m.lock();
        avail++;
        m.unlock();
        cv.notify_one();
    }

    size_t available() const {
        return avail;
    }
};


// TODO: I am aware this will cause read/write data races, but I don't think
// this matters too much in practice.
class BufferConsumer {
public:
    BufferConsumer(std::size_t size) {
        returnBuffer.resize(size);
        buffer1.resize(size);
        buffer2.resize(size);
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
            for (int i = 0; i < returnBuffer.size() && blockOnWrite; i++) {
                queue->wait_dequeue(returnBuffer[i]);
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
            if (offset >= buffer->size()) {
                {
                    juce::SpinLock::ScopedLockType scope(bufferLock);
                    buffer = buffer == &buffer1 ? &buffer2 : &buffer1;
                }
                offset = 0;
                sema.release();
            }
            
            (*buffer)[offset++] = point;
        }
    }

    std::vector<osci::Point>& getBuffer() {
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
    std::vector<osci::Point> returnBuffer;
    std::vector<osci::Point> buffer1;
    std::vector<osci::Point> buffer2;
    juce::SpinLock bufferLock;
    std::atomic<bool> blockOnWrite = false;
    std::vector<osci::Point>* buffer = &buffer1;
    Semaphore sema{0};
    int offset = 0;
};

} // namespace osci