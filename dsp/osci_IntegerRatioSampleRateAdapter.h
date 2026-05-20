/*
  ==============================================================================

   This file is part of the osci-render Addon module
   Copyright (c) 2025 James H Ball

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#ifndef OSCI_PROPRIETARY_BUILD
#define OSCI_PROPRIETARY_BUILD 0
#endif

#ifndef OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#define OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING 0
#endif

#if OSCI_PROPRIETARY_BUILD && OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#error "OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING cannot be enabled in OSCI_PROPRIETARY_BUILD."
#endif

#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#if !__has_include(<chowdsp_dsp_utils/chowdsp_dsp_utils.h>)
#error "OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING=1 requires the ChowDSP modules in the consuming project."
#endif
#include <chowdsp_dsp_utils/chowdsp_dsp_utils.h>
#endif

namespace osci
{

class IntegerRatioSampleRateAdapter
{
public:
    IntegerRatioSampleRateAdapter() = default;

    struct Spec
    {
        double deviceSampleRate = 0.0;
        double ratio = 1.0;
        int maxDeviceBlockSize = 0;
        int numChannels = 0;
    };

    struct ProcessResult
    {
        bool active = false;
        int internalSamplesProcessed = 0;
        int underflowSamples = 0;
        int overflowSamples = 0;
    };

    static constexpr double minProcessingSampleRate = 20000.0;
    static constexpr double maxProcessingSampleRate = 1000000.0;

    static const std::array<double, 6>& getSupportedRatios() noexcept;
    static bool isRatioSupported (double ratio) noexcept;
    static bool isRatioAllowed (double deviceSampleRate, double ratio) noexcept;
    static double normaliseRatio (double ratio) noexcept;

    void prepare (const Spec&);
    void reset() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return ratio != 1.0; }
    [[nodiscard]] double getRatio() const noexcept { return ratio; }
    [[nodiscard]] double getProcessingSampleRate() const noexcept { return processingSampleRate; }
    [[nodiscard]] int getMaxProcessingBlockSize() const noexcept { return maxProcessingBlockSize; }
    [[nodiscard]] int getLatencySamples() const noexcept { return latencySamples; }

    template <typename ProcessInternal>
    ProcessResult process (juce::AudioBuffer<float>& deviceBuffer,
                           juce::MidiBuffer& deviceMidi,
                           ProcessInternal&& processInternal) noexcept;

private:
#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
    using Downsampler = chowdsp::Downsampler<float, chowdsp::ButterworthFilter<8>, false>;
    using Upsampler = chowdsp::Upsampler<float, chowdsp::ButterworthFilter<8>, false>;

    class AudioFifo
    {
    public:
        void prepare (int channels, int capacitySamples);
        void reset() noexcept;
        [[nodiscard]] int getNumReady() const noexcept { return fifo.getNumReady(); }
        int writeSilence (int numSamples) noexcept;
        int writeFrom (const juce::AudioBuffer<float>& source, int sourceStartSample, int numSamples) noexcept;
        int readInto (juce::AudioBuffer<float>& destination, int destinationStartSample, int numSamples) noexcept;

    private:
        juce::AudioBuffer<float> buffer;
        juce::AbstractFifo fifo { 1 };
    };

    enum class Mode
    {
        Bypass,
        Oversample,
        Undersample
    };

    static int integerPowerOfTwo (double value) noexcept;
    static int ratioToOversamplingStages (int factor) noexcept;

    void appendMidi (const juce::MidiBuffer& deviceMidi, int deviceNumSamples) noexcept;
    void movePendingMidiForInternalBlock (int internalNumSamples) noexcept;
    template <typename ProcessInternal>
    ProcessResult processOversampled (juce::AudioBuffer<float>& deviceBuffer,
                                      juce::MidiBuffer& deviceMidi,
                                      ProcessInternal&& processInternal) noexcept;

    template <typename ProcessInternal>
    ProcessResult processUndersampled (juce::AudioBuffer<float>& deviceBuffer,
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

    Downsampler downsampler;
    Upsampler upsampler;
    AudioFifo inputFifo;
    AudioFifo outputFifo;
    juce::AudioBuffer<float> underInputBuffer;
    juce::AudioBuffer<float> underProcessingBuffer;
    juce::AudioBuffer<float> underOutputBuffer;

    juce::MidiBuffer pendingMidi;
    juce::MidiBuffer midiScratch;
    juce::MidiBuffer internalMidi;
#else
    double deviceSampleRate = 0.0;
    double processingSampleRate = 0.0;
    double ratio = 1.0;
    int maxDeviceBlockSize = 0;
    int maxProcessingBlockSize = 0;
    int numChannels = 0;
    int latencySamples = 0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntegerRatioSampleRateAdapter)
};

template <typename ProcessInternal>
IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::process (
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept
{
#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
    if (mode == Mode::Oversample)
        return processOversampled (deviceBuffer, deviceMidi, std::forward<ProcessInternal> (processInternal));

    if (mode == Mode::Undersample)
        return processUndersampled (deviceBuffer, deviceMidi, std::forward<ProcessInternal> (processInternal));
#endif

    ProcessResult result;
    processInternal (deviceBuffer, deviceMidi);
    result.internalSamplesProcessed = deviceBuffer.getNumSamples();
    return result;
}

#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
template <typename ProcessInternal>
inline IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::processOversampled (
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept
{
    ProcessResult result;
    result.active = true;

    const auto deviceNumSamples = deviceBuffer.getNumSamples();
    if (deviceNumSamples > maxDeviceBlockSize || deviceBuffer.getNumChannels() > numChannels)
    {
        result.underflowSamples = deviceNumSamples;
        deviceBuffer.clear();
        deviceMidi.clear();
        return result;
    }

    appendMidi (deviceMidi, deviceNumSamples);
    deviceSamplesSeen += deviceNumSamples;

    juce::dsp::AudioBlock<const float> inputBlock { deviceBuffer };
    juce::dsp::AudioBlock<float> outputBlock { deviceBuffer };
    auto internalBlockView = oversampler->processSamplesUp (inputBlock);
    const auto internalNumSamples = (int) internalBlockView.getNumSamples();

    movePendingMidiForInternalBlock (internalNumSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        channelPointers[(size_t) channel] = internalBlockView.getChannelPointer ((size_t) channel);

    juce::AudioBuffer<float> internalBuffer { channelPointers.data(), numChannels, internalNumSamples };
    processInternal (internalBuffer, internalMidi);

    oversampler->processSamplesDown (outputBlock);
    processingSamplesSeen += internalNumSamples;
    result.internalSamplesProcessed = internalNumSamples;
    deviceMidi.clear();
    return result;
}

template <typename ProcessInternal>
inline IntegerRatioSampleRateAdapter::ProcessResult IntegerRatioSampleRateAdapter::processUndersampled (
    juce::AudioBuffer<float>& deviceBuffer,
    juce::MidiBuffer& deviceMidi,
    ProcessInternal&& processInternal) noexcept
{
    ProcessResult result;
    result.active = true;

    const auto deviceNumSamples = deviceBuffer.getNumSamples();
    if (deviceNumSamples > maxDeviceBlockSize || deviceBuffer.getNumChannels() > numChannels)
    {
        result.underflowSamples = deviceNumSamples;
        deviceBuffer.clear();
        deviceMidi.clear();
        return result;
    }

    appendMidi (deviceMidi, deviceNumSamples);
    deviceSamplesSeen += deviceNumSamples;
    result.overflowSamples += deviceNumSamples - inputFifo.writeFrom (deviceBuffer, 0, deviceNumSamples);

    const auto deviceSamplesToConvert = (inputFifo.getNumReady() / factor) * factor;
    if (deviceSamplesToConvert > 0)
    {
        inputFifo.readInto (underInputBuffer, 0, deviceSamplesToConvert);
        const auto internalNumSamples = deviceSamplesToConvert / factor;

        for (int channel = 0; channel < numChannels; ++channel)
            downsampler.process (underInputBuffer.getReadPointer (channel),
                                 underProcessingBuffer.getWritePointer (channel),
                                 channel,
                                 deviceSamplesToConvert);

        movePendingMidiForInternalBlock (internalNumSamples);
        juce::AudioBuffer<float> internalBuffer {
            underProcessingBuffer.getArrayOfWritePointers(),
            numChannels,
            internalNumSamples
        };
        processInternal (internalBuffer, internalMidi);
        processingSamplesSeen += internalNumSamples;
        result.internalSamplesProcessed = internalNumSamples;

        for (int channel = 0; channel < numChannels; ++channel)
            upsampler.process (underProcessingBuffer.getReadPointer (channel),
                               underOutputBuffer.getWritePointer (channel),
                               channel,
                               internalNumSamples);

        const auto deviceSamplesGenerated = internalNumSamples * factor;
        result.overflowSamples += deviceSamplesGenerated - outputFifo.writeFrom (underOutputBuffer, 0, deviceSamplesGenerated);
    }

    const auto samplesRead = outputFifo.readInto (deviceBuffer, 0, deviceNumSamples);
    result.underflowSamples += deviceNumSamples - samplesRead;
    deviceMidi.clear();
    return result;
}
#endif

} // namespace osci
