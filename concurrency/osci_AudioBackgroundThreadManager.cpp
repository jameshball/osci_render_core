#pragma once

#include <JuceHeader.h>
#include "osci_AudioBackgroundThread.h"
#include "osci_AudioBackgroundThreadManager.h"

namespace osci {

void AudioBackgroundThreadManager::registerThread(AudioBackgroundThread* thread) {
    juce::SpinLock::ScopedLockType scope(lock);
    threads.push_back(thread);
}

void AudioBackgroundThreadManager::unregisterThread(AudioBackgroundThread* thread) {
    juce::SpinLock::ScopedLockType scope(lock);
    threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
}

void AudioBackgroundThreadManager::write(const osci::Point& point) {
    juce::SpinLock::ScopedLockType scope(lock);
    for (auto& thread : threads) {
        thread->write(point);
    }
}

void AudioBackgroundThreadManager::write(const osci::Point& point, juce::String name) {
    juce::SpinLock::ScopedLockType scope(lock);
    for (auto& thread : threads) {
        if (thread->getThreadName().contains(name)) {
            thread->write(point);
        }
    }
}

void AudioBackgroundThreadManager::prepare(double sampleRate, int samplesPerBlock) {
    juce::SpinLock::ScopedLockType scope(lock);
    for (auto& thread : threads) {
        thread->prepare(sampleRate, samplesPerBlock);
    }
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
}

} // namespace osci