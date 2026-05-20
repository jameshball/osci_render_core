#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstdint>
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

    void appendMidi(const juce::MidiBuffer& deviceMidi, int deviceNumSamples) noexcept;
    void movePendingMidiForInternalBlock(int internalNumSamples) noexcept;

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

    int64_t deviceSamplesSeen = 0;
    int64_t processingSamplesSeen = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<float*> channelPointers;

    juce::MidiBuffer pendingMidi;
    juce::MidiBuffer midiScratch;
    juce::MidiBuffer internalMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntegerRatioSampleRateAdapter)
};

template <typename ProcessInternal>
IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::process(
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept {
    if (mode == Mode::Upsample) {
        return processUpsampled(deviceBuffer, deviceMidi, std::forward<ProcessInternal>(processInternal));
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

    appendMidi(deviceMidi, deviceNumSamples);
    deviceSamplesSeen += deviceNumSamples;

    juce::dsp::AudioBlock<const float> inputBlock { deviceBuffer };
    juce::dsp::AudioBlock<float> outputBlock { deviceBuffer };
    auto internalBlockView = oversampler->processSamplesUp(inputBlock);
    const auto internalNumSamples = (int) internalBlockView.getNumSamples();

    movePendingMidiForInternalBlock(internalNumSamples);
    for (int channel = 0; channel < numChannels; ++channel) {
        channelPointers[(size_t) channel] = internalBlockView.getChannelPointer((size_t) channel);
    }

    juce::AudioBuffer<float> internalBuffer { channelPointers.data(), numChannels, internalNumSamples };
    processInternal(internalBuffer, internalMidi);

    oversampler->processSamplesDown(outputBlock);
    processingSamplesSeen += internalNumSamples;
    result.internalSamplesProcessed = internalNumSamples;
    deviceMidi.clear();
    return result;
}

} // namespace osci
