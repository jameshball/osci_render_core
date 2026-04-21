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
    // Takes juce::StringRef to avoid heap-allocating a juce::String on the audio
    // thread from a const char* literal at the call site (was causing ~50% of
    // audio-thread CPU time during the first seconds of playback).
    void write(juce::AudioBuffer<float>& buffer, juce::StringRef name);
    void prepare(double sampleRate, int samplesPerBlock);
    
    double sampleRate = 44100.0;
    int samplesPerBlock = 128;

private:
    juce::SpinLock lock;
    std::vector<AudioBackgroundThread*> threads;
};

} // namespace osci
