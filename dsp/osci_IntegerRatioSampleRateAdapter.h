#pragma once

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <utility>
#include <vector>

namespace osci {

class IntegerRatioSampleRateAdapter {
public:
    IntegerRatioSampleRateAdapter() = default;

    struct Spec {
        double deviceSampleRate = 0.0;
        double ratio = 1.0;
        int maxDeviceBlockSize = 0;
        int numChannels = 0;
    };

    struct ProcessResult {
        bool active = false;
        int internalSamplesProcessed = 0;
    };

    static constexpr double minProcessingSampleRate = 20000.0;
    static constexpr double maxProcessingSampleRate = 1000000.0;

    static const std::array<double, 4>& getSupportedRatios() noexcept;
    static bool isRatioSupported(double ratio) noexcept;
    static bool isRatioAllowed(double deviceSampleRate, double ratio) noexcept;
    static double normaliseRatio(double ratio) noexcept;

    void prepare(const Spec&);
    void reset() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return ratio != 1.0; }
    [[nodiscard]] double getRatio() const noexcept { return ratio; }
    [[nodiscard]] double getProcessingSampleRate() const noexcept { return processingSampleRate; }
    [[nodiscard]] int getMaxProcessingBlockSize() const noexcept { return maxProcessingBlockSize; }
    [[nodiscard]] int getLatencySamples() const noexcept { return latencySamples; }

    template <typename ProcessInternal>
    ProcessResult process(juce::AudioBuffer<float>& deviceBuffer,
                          juce::MidiBuffer& deviceMidi,
                          ProcessInternal&& processInternal) noexcept;

private:
    enum class Mode {
        Bypass,
        Upsample
    };

    static int integerPowerOfTwo(double value) noexcept;
    static int ratioToOversamplingStages(int factor) noexcept;

    template <typename ProcessInternal>
    ProcessResult processUpsampled(juce::AudioBuffer<float>& deviceBuffer,
                                   juce::MidiBuffer& deviceMidi,
                                   ProcessInternal&& processInternal) noexcept;

    Mode mode = Mode::Bypass;
    double deviceSampleRate = 0.0;
    double processingSampleRate = 0.0;
    double ratio = 1.0;
    int factor = 1;
    int maxDeviceBlockSize = 0;
    int maxProcessingBlockSize = 0;
    int numChannels = 0;
    int latencySamples = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<float*> channelPointers;

    juce::MidiBuffer internalMidi;
    juce::MidiBuffer pendingZeroBlockMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntegerRatioSampleRateAdapter)
};

template <typename ProcessInternal>
IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::process(
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept {
    if (deviceBuffer.getNumSamples() == 0) {
        for (const auto metadata : deviceMidi) {
            pendingZeroBlockMidi.addEvent(metadata.data, metadata.numBytes, 0);
        }
        deviceMidi.clear();
        return { mode == Mode::Upsample, 0 };
    }
    if (mode == Mode::Upsample) {
        return processUpsampled(deviceBuffer, deviceMidi, std::forward<ProcessInternal>(processInternal));
    }

    if (!pendingZeroBlockMidi.isEmpty()) {
        pendingZeroBlockMidi.addEvents(deviceMidi, 0, -1, 0);
        deviceMidi.clear();
        deviceMidi.addEvents(pendingZeroBlockMidi, 0, -1, 0);
        pendingZeroBlockMidi.clear();
    }
    ProcessResult result;
    processInternal(deviceBuffer, deviceMidi);
    result.internalSamplesProcessed = deviceBuffer.getNumSamples();
    return result;
}

template <typename ProcessInternal>
inline IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::processUpsampled(
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept {
    ProcessResult result;
    result.active = true;

    const auto deviceNumSamples = deviceBuffer.getNumSamples();
    if (deviceNumSamples > maxDeviceBlockSize || deviceBuffer.getNumChannels() > numChannels) {
        deviceBuffer.clear();
        deviceMidi.clear();
        return result;
    }

    internalMidi.clear();
    internalMidi.addEvents(pendingZeroBlockMidi, 0, -1, 0);
    pendingZeroBlockMidi.clear();
    for (const auto metadata : deviceMidi) {
        const auto position = juce::jlimit(0, juce::jmax(0, deviceNumSamples - 1), metadata.samplePosition);
        internalMidi.addEvent(metadata.data, metadata.numBytes, position * factor);
    }

    juce::dsp::AudioBlock<const float> inputBlock { deviceBuffer };
    juce::dsp::AudioBlock<float> outputBlock { deviceBuffer };
    auto internalBlockView = oversampler->processSamplesUp(inputBlock);
    const auto internalNumSamples = (int) internalBlockView.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel) {
        channelPointers[(size_t) channel] = internalBlockView.getChannelPointer((size_t) channel);
    }

    juce::AudioBuffer<float> internalBuffer { channelPointers.data(), numChannels, internalNumSamples };
    processInternal(internalBuffer, internalMidi);

    oversampler->processSamplesDown(outputBlock);
    result.internalSamplesProcessed = internalNumSamples;
    deviceMidi.clear();
    return result;
}

} // namespace osci
