#pragma once

#include <JuceHeader.h>
#include "osci_BufferConsumer.h"

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
    void start();
    void stop();
    
    AudioBackgroundThreadManager& manager;
    std::unique_ptr<BufferConsumer> consumer = nullptr;
    std::atomic<bool> shouldBeRunning = false;
    std::atomic<bool> isPrepared = false;
    std::atomic<bool> deleting = false;

protected:
    
    virtual int prepareTask(double sampleRate, int samplesPerBlock) = 0;
    virtual void runTask(const juce::AudioBuffer<float>& buffer) = 0;
    virtual void stopTask() = 0;
};

} // namespace osci