#include "osci_IntegerRatioSampleRateAdapter.h"

#include <cmath>
#include <limits>

namespace osci {

const std::array<double, 4>& IntegerRatioSampleRateAdapter::getSupportedRatios() noexcept {
    static constexpr std::array<double, 4> ratios { 1.0, 2.0, 4.0, 8.0 };
    return ratios;
}

bool IntegerRatioSampleRateAdapter::isRatioSupported(double value) noexcept {
    for (const auto supported : getSupportedRatios()) {
        if (std::abs(value - supported) < 0.000001) {
            return true;
        }
    }

    return false;
}

double IntegerRatioSampleRateAdapter::normaliseRatio(double value) noexcept {
    auto best = 1.0;
    auto bestDistance = std::numeric_limits<double>::max();
    for (const auto supported : getSupportedRatios()) {
        const auto distance = std::abs(value - supported);
        if (distance < bestDistance) {
            best = supported;
            bestDistance = distance;
        }
    }

    return best;
}

bool IntegerRatioSampleRateAdapter::isRatioAllowed(double sampleRate, double value) noexcept {
    if (!isRatioSupported(value)) {
        return false;
    }

    if (sampleRate <= 0.0) {
        return value == 1.0;
    }

    const auto processingRate = sampleRate * normaliseRatio(value);
    return processingRate >= minProcessingSampleRate && processingRate <= maxProcessingSampleRate;
}

int IntegerRatioSampleRateAdapter::integerPowerOfTwo(double value) noexcept {
    return juce::roundToInt(normaliseRatio(value));
}

int IntegerRatioSampleRateAdapter::ratioToOversamplingStages(int value) noexcept {
    auto stages = 0;
    while (value > 1) {
        value >>= 1;
        ++stages;
    }

    return stages;
}

void IntegerRatioSampleRateAdapter::prepare(const Spec& spec) {
    deviceSampleRate = spec.deviceSampleRate;
    ratio = isRatioAllowed(deviceSampleRate, spec.ratio) ? normaliseRatio(spec.ratio) : 1.0;
    processingSampleRate = deviceSampleRate > 0.0 ? deviceSampleRate * ratio : 0.0;
    maxDeviceBlockSize = juce::jmax(1, spec.maxDeviceBlockSize);
    numChannels = juce::jmax(1, spec.numChannels);
    factor = integerPowerOfTwo(ratio);
    mode = ratio > 1.0 ? Mode::Upsample : Mode::Bypass;

    pendingMidi.ensureSize((size_t) juce::jmax(4096, maxDeviceBlockSize * 256));
    midiScratch.ensureSize((size_t) juce::jmax(4096, maxDeviceBlockSize * 256));
    internalMidi.ensureSize((size_t) juce::jmax(4096, maxDeviceBlockSize * 256));

    if (mode == Mode::Upsample) {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
            (size_t) numChannels,
            (size_t) ratioToOversamplingStages(factor),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            false,
            true);
        oversampler->initProcessing((size_t) maxDeviceBlockSize);
        maxProcessingBlockSize = maxDeviceBlockSize * factor;
        latencySamples = (int) std::ceil(oversampler->getLatencyInSamples());
        channelPointers.resize((size_t) numChannels);
    } else {
        oversampler.reset();
        channelPointers.clear();
        maxProcessingBlockSize = maxDeviceBlockSize;
        latencySamples = 0;
    }

    reset();
}

void IntegerRatioSampleRateAdapter::reset() noexcept {
    if (oversampler != nullptr) {
        oversampler->reset();
    }

    pendingMidi.clear();
    midiScratch.clear();
    internalMidi.clear();
    deviceSamplesSeen = 0;
    processingSamplesSeen = 0;
}

void IntegerRatioSampleRateAdapter::appendMidi(const juce::MidiBuffer& deviceMidi, int deviceNumSamples) noexcept {
    const auto maxDevicePosition = juce::jmax(0, deviceNumSamples - 1);
    for (const auto metadata : deviceMidi) {
        const auto absoluteDeviceSample = deviceSamplesSeen + (int64_t) juce::jlimit(0, maxDevicePosition, metadata.samplePosition);
        const auto absoluteInternalSample = absoluteDeviceSample * factor;
        const auto relativeInternalSample = absoluteInternalSample - processingSamplesSeen;

        if (relativeInternalSample >= 0 && relativeInternalSample <= (int64_t) std::numeric_limits<int>::max()) {
            pendingMidi.addEvent(metadata.getMessage(), (int) relativeInternalSample);
        }
    }
}

void IntegerRatioSampleRateAdapter::movePendingMidiForInternalBlock(int internalNumSamples) noexcept {
    internalMidi.clear();
    midiScratch.clear();

    for (const auto metadata : pendingMidi) {
        if (metadata.samplePosition < internalNumSamples) {
            internalMidi.addEvent(metadata.getMessage(), juce::jlimit(0, juce::jmax(0, internalNumSamples - 1), metadata.samplePosition));
        } else {
            midiScratch.addEvent(metadata.getMessage(), metadata.samplePosition - internalNumSamples);
        }
    }

    pendingMidi.swapWith(midiScratch);
    midiScratch.clear();
}

} // namespace osci
