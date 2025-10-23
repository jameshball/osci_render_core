#pragma once

#include <JuceHeader.h>

namespace osci {

class AudioBackgroundThread;
class AudioBackgroundThreadManager {
public:
    AudioBackgroundThreadManager() {}
    ~AudioBackgroundThreadManager() {}
    
    void registerThread(AudioBackgroundThread* thread);
    void unregisterThread(AudioBackgroundThread* thread);
    void write(juce::AudioBuffer<float>& buffer);
    void write(juce::AudioBuffer<float>& buffer, juce::String name);
    void prepare(double sampleRate, int samplesPerBlock);
    
    double sampleRate = 44100.0;
    int samplesPerBlock = 128;

private:
    juce::SpinLock lock;
    std::vector<AudioBackgroundThread*> threads;
};

} // namespace osci
