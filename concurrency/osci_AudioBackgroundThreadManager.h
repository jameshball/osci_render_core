#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace osci {

class AudioBackgroundThread;
class AudioBackgroundThreadManager {
public:
    AudioBackgroundThreadManager() {}
    ~AudioBackgroundThreadManager() {}
    
    void write(juce::AudioBuffer<float>& buffer);
    // Takes juce::StringRef to avoid heap-allocating a juce::String on the audio
    // thread from a const char* literal at the call site (was causing ~50% of
    // audio-thread CPU time during the first seconds of playback).
    void write(juce::AudioBuffer<float>& buffer, juce::StringRef name);
    void prepare(double sampleRate, int samplesPerBlock);
    
    double sampleRate = 44100.0;
    int samplesPerBlock = 128;

private:
    friend class AudioBackgroundThread;
    void unregisterThread(AudioBackgroundThread* thread);
    // When both locks are needed, take lock before lifecycleLock. A recording writer
    // can hold lock while blocked, so stop/mode changes take only lifecycleLock
    // to remain able to release it. Audio writes never take lifecycleLock.
    juce::SpinLock lifecycleLock;
    juce::SpinLock lock;
    std::vector<AudioBackgroundThread*> threads;
};

} // namespace osci
