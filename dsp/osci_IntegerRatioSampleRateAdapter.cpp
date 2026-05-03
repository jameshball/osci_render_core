/*
  ==============================================================================

   This file is part of the osci-render Addon module
   Copyright (c) 2025 James H Ball

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

  ==============================================================================
*/

#include "osci_IntegerRatioSampleRateAdapter.h"

#include <cmath>
#include <limits>

namespace osci
{

const std::array<double, 6>& IntegerRatioSampleRateAdapter::getSupportedRatios() noexcept
{
    static constexpr std::array<double, 6> ratios { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
    return ratios;
}

bool IntegerRatioSampleRateAdapter::isRatioSupported (double value) noexcept
{
    for (const auto supported : getSupportedRatios())
        if (std::abs (value - supported) < 0.000001)
            return true;

    return false;
}

double IntegerRatioSampleRateAdapter::normaliseRatio (double value) noexcept
{
    auto best = 1.0;
    auto bestDistance = std::numeric_limits<double>::max();
    for (const auto supported : getSupportedRatios())
    {
        const auto distance = std::abs (value - supported);
        if (distance < bestDistance)
        {
            best = supported;
            bestDistance = distance;
        }
    }

    return best;
}

bool IntegerRatioSampleRateAdapter::isRatioAllowed (double sampleRate, double value) noexcept
{
    if (! isRatioSupported (value))
        return false;

    if (sampleRate <= 0.0)
        return value == 1.0;

    const auto processingRate = sampleRate * normaliseRatio (value);
    return processingRate >= minProcessingSampleRate && processingRate <= maxProcessingSampleRate;
}

void IntegerRatioSampleRateAdapter::AudioFifo::prepare (int channels, int capacitySamples)
{
    buffer.setSize (juce::jmax (1, channels), juce::jmax (1, capacitySamples) + 1, false, true, true);
    fifo.setTotalSize (buffer.getNumSamples());
    reset();
}

void IntegerRatioSampleRateAdapter::AudioFifo::reset() noexcept
{
    fifo.reset();
    buffer.clear();
}

int IntegerRatioSampleRateAdapter::AudioFifo::writeSilence (int numSamples) noexcept
{
    const auto samplesToWrite = juce::jlimit (0, fifo.getFreeSpace(), numSamples);
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (samplesToWrite, start1, size1, start2, size2);

    if (size1 > 0) buffer.clear (start1, size1);
    if (size2 > 0) buffer.clear (start2, size2);

    fifo.finishedWrite (size1 + size2);
    return size1 + size2;
}

int IntegerRatioSampleRateAdapter::AudioFifo::writeFrom (const juce::AudioBuffer<float>& source,
                                                         int sourceStartSample,
                                                         int numSamples) noexcept
{
    const auto samplesToWrite = juce::jlimit (0, fifo.getFreeSpace(), numSamples);
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (samplesToWrite, start1, size1, start2, size2);

    const auto copy = [&] (int destinationStart, int sourceStart, int count)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            if (channel < source.getNumChannels())
                buffer.copyFrom (channel, destinationStart, source, channel, sourceStart, count);
            else
                buffer.clear (channel, destinationStart, count);
        }
    };

    if (size1 > 0) copy (start1, sourceStartSample, size1);
    if (size2 > 0) copy (start2, sourceStartSample + size1, size2);

    fifo.finishedWrite (size1 + size2);
    return size1 + size2;
}

int IntegerRatioSampleRateAdapter::AudioFifo::readInto (juce::AudioBuffer<float>& destination,
                                                        int destinationStartSample,
                                                        int numSamples) noexcept
{
    const auto samplesToRead = juce::jlimit (0, fifo.getNumReady(), numSamples);
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (samplesToRead, start1, size1, start2, size2);

    const auto copy = [&] (int sourceStart, int destinationStart, int count)
    {
        for (int channel = 0; channel < destination.getNumChannels(); ++channel)
        {
            if (channel < buffer.getNumChannels())
                destination.copyFrom (channel, destinationStart, buffer, channel, sourceStart, count);
            else
                destination.clear (channel, destinationStart, count);
        }
    };

    if (size1 > 0) copy (start1, destinationStartSample, size1);
    if (size2 > 0) copy (start2, destinationStartSample + size1, size2);

    fifo.finishedRead (size1 + size2);

    const auto samplesRead = size1 + size2;
    if (samplesRead < numSamples)
        destination.clear (destinationStartSample + samplesRead, numSamples - samplesRead);

    return samplesRead;
}

int IntegerRatioSampleRateAdapter::integerPowerOfTwo (double value) noexcept
{
    return juce::roundToInt (normaliseRatio (value));
}

int IntegerRatioSampleRateAdapter::ratioToOversamplingStages (int value) noexcept
{
    auto stages = 0;
    while (value > 1)
    {
        value >>= 1;
        ++stages;
    }

    return stages;
}

void IntegerRatioSampleRateAdapter::prepare (const Spec& spec)
{
    deviceSampleRate = spec.deviceSampleRate;
    ratio = isRatioAllowed (deviceSampleRate, spec.ratio) ? normaliseRatio (spec.ratio) : 1.0;
    processingSampleRate = deviceSampleRate > 0.0 ? deviceSampleRate * ratio : 0.0;
    maxDeviceBlockSize = juce::jmax (1, spec.maxDeviceBlockSize);
    numChannels = juce::jmax (1, spec.numChannels);
    factor = ratio >= 1.0 ? integerPowerOfTwo (ratio) : integerPowerOfTwo (1.0 / ratio);
    mode = ratio > 1.0 ? Mode::Oversample : (ratio < 1.0 ? Mode::Undersample : Mode::Bypass);

    pendingMidi.ensureSize ((size_t) juce::jmax (4096, maxDeviceBlockSize * 256));
    midiScratch.ensureSize ((size_t) juce::jmax (4096, maxDeviceBlockSize * 256));
    internalMidi.ensureSize ((size_t) juce::jmax (4096, maxDeviceBlockSize * 256));

    if (mode == Mode::Oversample)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels,
            (size_t) ratioToOversamplingStages (factor),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            false,
            true);
        oversampler->initProcessing ((size_t) maxDeviceBlockSize);
        maxProcessingBlockSize = maxDeviceBlockSize * factor;
        latencySamples = (int) std::ceil (oversampler->getLatencyInSamples());
        channelPointers.resize ((size_t) numChannels);
    }
    else if (mode == Mode::Undersample)
    {
        const auto inputCapacity = maxDeviceBlockSize + factor - 1;
        maxProcessingBlockSize = juce::jmax (1, inputCapacity / factor);
        latencySamples = factor;

        downsampler.prepare ({ deviceSampleRate, (juce::uint32) inputCapacity, (juce::uint32) numChannels }, factor);
        upsampler.prepare ({ processingSampleRate, (juce::uint32) maxProcessingBlockSize, (juce::uint32) numChannels }, factor);

        inputFifo.prepare (numChannels, inputCapacity + maxDeviceBlockSize);
        outputFifo.prepare (numChannels, latencySamples + inputCapacity + maxDeviceBlockSize);
        underInputBuffer.setSize (numChannels, inputCapacity, false, true, true);
        underProcessingBuffer.setSize (numChannels, maxProcessingBlockSize, false, true, true);
        underOutputBuffer.setSize (numChannels, inputCapacity, false, true, true);
        oversampler.reset();
        channelPointers.clear();
    }
    else
    {
        oversampler.reset();
        channelPointers.clear();
        maxProcessingBlockSize = maxDeviceBlockSize;
        latencySamples = 0;
    }

    reset();
}

void IntegerRatioSampleRateAdapter::reset() noexcept
{
    if (oversampler != nullptr)
        oversampler->reset();

    downsampler.reset();
    upsampler.reset();
    inputFifo.reset();
    outputFifo.reset();

    if (mode == Mode::Undersample)
        outputFifo.writeSilence (latencySamples);

    pendingMidi.clear();
    midiScratch.clear();
    internalMidi.clear();
    deviceSamplesSeen = 0;
    processingSamplesSeen = 0;
}

void IntegerRatioSampleRateAdapter::appendMidi (const juce::MidiBuffer& deviceMidi, int deviceNumSamples) noexcept
{
    const auto maxDevicePosition = juce::jmax (0, deviceNumSamples - 1);
    for (const auto metadata : deviceMidi)
    {
        const auto absoluteDeviceSample = deviceSamplesSeen + (int64_t) juce::jlimit (0, maxDevicePosition, metadata.samplePosition);
        const auto absoluteInternalSample = mode == Mode::Undersample
            ? absoluteDeviceSample / factor
            : absoluteDeviceSample * factor;
        const auto relativeInternalSample = absoluteInternalSample - processingSamplesSeen;

        if (relativeInternalSample >= 0 && relativeInternalSample <= (int64_t) std::numeric_limits<int>::max())
            pendingMidi.addEvent (metadata.getMessage(), (int) relativeInternalSample);
    }
}

void IntegerRatioSampleRateAdapter::movePendingMidiForInternalBlock (int internalNumSamples) noexcept
{
    internalMidi.clear();
    midiScratch.clear();

    for (const auto metadata : pendingMidi)
    {
        if (metadata.samplePosition < internalNumSamples)
            internalMidi.addEvent (metadata.getMessage(), juce::jlimit (0, juce::jmax (0, internalNumSamples - 1), metadata.samplePosition));
        else
            midiScratch.addEvent (metadata.getMessage(), metadata.samplePosition - internalNumSamples);
    }

    pendingMidi.swapWith (midiScratch);
    midiScratch.clear();
}

} // namespace osci
