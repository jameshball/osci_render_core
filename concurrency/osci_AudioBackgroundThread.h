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
    void setShouldBeRunning(bool shouldBeRunning, std::function<void()> stopCallback = nullptr);
    void write(juce::AudioBuffer<float>& buffer);
    void setBlockOnAudioThread(bool block);
    
private:
    
    void run() override;
    int paceLiveTask(int offset, int batchSamples, double& nextFrameTime, unsigned revision);
    void start();
    void stop();
    
    AudioBackgroundThreadManager& manager;
    std::unique_ptr<BufferConsumer> consumer = nullptr;
    std::atomic<bool> shouldBeRunning = false;
    std::atomic<bool> isPrepared = false;
    std::atomic<bool> deleting = false;
    std::atomic<unsigned> taskRevision { 0 };
    int samplesPerTask = 1;
    double taskIntervalMs = 0.0;

protected:
    
    // Return samples per task. Larger audio callbacks are batched and paced in live mode only.
    virtual int prepareTask(double sampleRate, int samplesPerBlock) = 0;
    virtual void runTask(const juce::AudioBuffer<float>& buffer) = 0;
    virtual void stopTask() = 0;
};

} // namespace osci
