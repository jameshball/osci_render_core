#pragma once

#include "osci_AudioBackgroundThread.h"
#include "osci_AudioBackgroundThreadManager.h"

#include <algorithm>

namespace osci {

void AudioBackgroundThreadManager::unregisterThread(AudioBackgroundThread* thread) {
    juce::SpinLock::ScopedLockType scope(lock);
    juce::SpinLock::ScopedLockType lifecycleScope(lifecycleLock);
    threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
    thread->registered = false;
}

void AudioBackgroundThreadManager::write(juce::AudioBuffer<float>& buffer) {
    juce::SpinLock::ScopedLockType scope(lock);
    for (auto& thread : threads) {
        thread->write(buffer);
    }
}

void AudioBackgroundThreadManager::write(juce::AudioBuffer<float>& buffer, juce::StringRef name) {
    juce::SpinLock::ScopedLockType scope(lock);
    for (auto& thread : threads) {
        if (thread->getThreadName().contains(name)) {
            thread->write(buffer);
        }
    }
}

void AudioBackgroundThreadManager::prepare(double sampleRate, int samplesPerBlock) {
    juce::SpinLock::ScopedLockType scope(lock);
    juce::SpinLock::ScopedLockType lifecycleScope(lifecycleLock);
    for (auto& thread : threads) {
        thread->prepareInternal(sampleRate, samplesPerBlock);
    }
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
}

} // namespace osci
