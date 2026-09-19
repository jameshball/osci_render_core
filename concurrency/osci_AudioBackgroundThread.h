#pragma once

#include "osci_BufferConsumer.h"

#include <atomic>
#include <functional>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace osci {

class AudioBackgroundThreadManager;
class AudioBackgroundThread : public juce::Thread {
public:
    AudioBackgroundThread(const juce::String& name, AudioBackgroundThreadManager& manager);
    ~AudioBackgroundThread() override;
    
    void prepare(double sampleRate, int samplesPerBlock);
    // First start publishes this object: call only after derived initialization is complete.
    void setShouldBeRunning(bool shouldBeRunning, std::function<void()> stopCallback = nullptr);
    void write(juce::AudioBuffer<float>& buffer);
    void setBlockOnAudioThread(bool block);
    
private:
    friend class AudioBackgroundThreadManager;
    void prepareInternal(double sampleRate, int samplesPerBlock);
    void setShouldBeRunningInternal(bool shouldBeRunning, std::function<void()> stopCallback = nullptr);
    
    void run() override;
    int paceLiveTask(int offset, int batchSamples, double& nextFrameTime, unsigned revision);
    void start();
    void stop();
    
    AudioBackgroundThreadManager& manager;
    std::unique_ptr<BufferConsumer> consumer = nullptr;
    std::atomic<bool> shouldBeRunning = false;
    std::atomic<bool> isPrepared = false;
    std::atomic<bool> deleting = false;
    bool registered = false; // Protected by the manager's lifecycle lock.
    std::atomic<unsigned> taskRevision { 0 };
    int samplesPerTask = 1;
    double taskIntervalMs = 0.0;

protected:
    // Stop the worker, then call this in the most-derived destructor before member
    // teardown or a vtable change. Waits for manager callbacks already in flight.
    void unregisterFromManager();
    
    // Return samples per task. Larger audio callbacks are batched and paced in live mode only.
    virtual int prepareTask(double sampleRate, int samplesPerBlock) = 0;
    virtual void runTask(const juce::AudioBuffer<float>& buffer) = 0;
    virtual void stopTask() = 0;
};

} // namespace osci
