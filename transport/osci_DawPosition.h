#pragma once

#include <atomic>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace osci {

class DawPosition {
public:
    struct Options {
        double fallbackBpm = 60.0;
        std::optional<double> bpmOverride;

        static Options withBpmOverride(double bpm, double fallback = 60.0) {
            Options options;
            options.fallbackBpm = fallback;
            options.bpmOverride = bpm;
            return options;
        }
    };

    DawPosition() = default;

    DawPosition(const DawPosition& other) {
        storeFrom(other);
    }

    DawPosition(DawPosition&& other) noexcept {
        storeFrom(other);
    }

    DawPosition& operator=(const DawPosition& other) = delete;
    DawPosition& operator=(DawPosition&& other) = delete;

    void storeFrom(const DawPosition& other) {
        if (this != &other) {
            bpm.store(other.bpm.load(std::memory_order_relaxed), std::memory_order_relaxed);
            seconds.store(other.seconds.load(std::memory_order_relaxed), std::memory_order_relaxed);
            beats.store(other.beats.load(std::memory_order_relaxed), std::memory_order_relaxed);
            syncSeconds.store(other.syncSeconds.load(std::memory_order_relaxed), std::memory_order_relaxed);
            secondsPerSample.store(other.secondsPerSample.load(std::memory_order_relaxed), std::memory_order_relaxed);
            beatsPerSample.store(other.beatsPerSample.load(std::memory_order_relaxed), std::memory_order_relaxed);
            syncSecondsPerSample.store(other.syncSecondsPerSample.load(std::memory_order_relaxed), std::memory_order_relaxed);
            isPlaying.store(other.isPlaying.load(std::memory_order_relaxed), std::memory_order_relaxed);
            hasTimePosition.store(other.hasTimePosition.load(std::memory_order_relaxed), std::memory_order_relaxed);
            hasBeatPosition.store(other.hasBeatPosition.load(std::memory_order_relaxed), std::memory_order_relaxed);
            hasSyncPosition.store(other.hasSyncPosition.load(std::memory_order_relaxed), std::memory_order_relaxed);
            timeSigNumerator.store(other.timeSigNumerator.load(std::memory_order_relaxed), std::memory_order_relaxed);
            timeSigDenominator.store(other.timeSigDenominator.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }

    std::atomic<double> bpm{60.0};
    std::atomic<double> seconds{0.0};
    std::atomic<double> beats{0.0};
    std::atomic<double> syncSeconds{0.0};
    std::atomic<double> secondsPerSample{0.0};
    std::atomic<double> beatsPerSample{0.0};
    std::atomic<double> syncSecondsPerSample{0.0};
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> hasTimePosition{false};
    std::atomic<bool> hasBeatPosition{false};
    std::atomic<bool> hasSyncPosition{false};
    std::atomic<int> timeSigNumerator{4};
    std::atomic<int> timeSigDenominator{4};

    static DawPosition fromPlayHead(juce::AudioPlayHead* playHead, double sampleRate) {
        Options options;
        return fromPlayHead(playHead, sampleRate, options);
    }

    static DawPosition fromPlayHead(juce::AudioPlayHead* playHead,
                                    double sampleRate,
                                    const Options& options) {
        DawPosition position;
        position.bpm.store(options.fallbackBpm, std::memory_order_relaxed);
        position.setSampleRate(sampleRate);

        bool hasSourceBeatPosition = false;
        if (playHead != nullptr) {
            auto currentPosition = playHead->getPosition();
            if (currentPosition.hasValue()) {
                hasSourceBeatPosition = position.applyPositionInfo(*currentPosition);
            }
        }

        if (options.bpmOverride.has_value()) {
            position.bpm.store(*options.bpmOverride, std::memory_order_relaxed);
        }

        position.derivePositions(hasSourceBeatPosition);
        return position;
    }

    static DawPosition fromPositionInfo(const juce::AudioPlayHead::PositionInfo& positionInfo,
                                        double sampleRate) {
        Options options;
        return fromPositionInfo(positionInfo, sampleRate, options);
    }

    static DawPosition fromPositionInfo(const juce::AudioPlayHead::PositionInfo& positionInfo,
                                        double sampleRate,
                                        const Options& options) {
        DawPosition position;
        position.bpm.store(options.fallbackBpm, std::memory_order_relaxed);
        position.setSampleRate(sampleRate);

        const bool hasSourceBeatPosition = position.applyPositionInfo(positionInfo);
        if (options.bpmOverride.has_value()) {
            position.bpm.store(*options.bpmOverride, std::memory_order_relaxed);
        }

        position.derivePositions(hasSourceBeatPosition);
        return position;
    }

private:
    void setSampleRate(double sampleRate) {
        secondsPerSample.store(sampleRate > 0.0 ? 1.0 / sampleRate : 0.0, std::memory_order_relaxed);
    }

    bool applyPositionInfo(const juce::AudioPlayHead::PositionInfo& positionInfo) {
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        bpm.store(positionInfo.getBpm().orFallback(currentBpm), std::memory_order_relaxed);

        auto timeSeconds = positionInfo.getTimeInSeconds();
        if (timeSeconds.hasValue()) {
            seconds.store(*timeSeconds, std::memory_order_relaxed);
            hasTimePosition.store(true, std::memory_order_relaxed);
        }

        bool hasSourceBeatPosition = false;
        auto ppq = positionInfo.getPpqPosition();
        if (ppq.hasValue()) {
            beats.store(*ppq, std::memory_order_relaxed);
            hasBeatPosition.store(true, std::memory_order_relaxed);
            hasSourceBeatPosition = true;
        }

        isPlaying.store(positionInfo.getIsPlaying(), std::memory_order_relaxed);
        const juce::AudioPlayHead::TimeSignature fallbackTimeSignature {
            timeSigNumerator.load(std::memory_order_relaxed),
            timeSigDenominator.load(std::memory_order_relaxed)
        };
        const auto timeSignature = positionInfo.getTimeSignature().orFallback(fallbackTimeSignature);
        timeSigNumerator.store(timeSignature.numerator, std::memory_order_relaxed);
        timeSigDenominator.store(timeSignature.denominator, std::memory_order_relaxed);
        return hasSourceBeatPosition;
    }

    void derivePositions(bool hasSourceBeatPosition) {
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        const double currentSeconds = seconds.load(std::memory_order_relaxed);
        const double currentSecondsPerSample = secondsPerSample.load(std::memory_order_relaxed);
        if (!hasSourceBeatPosition && hasTimePosition.load(std::memory_order_relaxed)) {
            beats.store(currentBpm * currentSeconds / 60.0, std::memory_order_relaxed);
            hasBeatPosition.store(true, std::memory_order_relaxed);
        }

        beatsPerSample.store(currentBpm * currentSecondsPerSample / 60.0, std::memory_order_relaxed);
        syncSecondsPerSample.store(currentSecondsPerSample, std::memory_order_relaxed);

        if (hasBeatPosition.load(std::memory_order_relaxed) && currentBpm > 0.0) {
            syncSeconds.store(beats.load(std::memory_order_relaxed) / (currentBpm / 60.0), std::memory_order_relaxed);
            hasSyncPosition.store(true, std::memory_order_relaxed);
        } else if (hasTimePosition.load(std::memory_order_relaxed)) {
            syncSeconds.store(currentSeconds, std::memory_order_relaxed);
            hasSyncPosition.store(true, std::memory_order_relaxed);
        }
    }
};

} // namespace osci
