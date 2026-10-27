#pragma once
#include "../effect/osci_SimpleEffect.h"

#include <cmath>

class TraceEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<TraceEffect>();
	}

	void prepareToPlay(float sampleRate) override {
		buffer.resize((int)std::ceil(sampleRate));
		bufferIndex = 0;
		samplesSinceFrameStart = 0;
	}

	void onFrameStart() override {
		samplesSinceFrameStart = 0;
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		if (buffer.empty()) return input;
        buffer[bufferIndex] = input;
        
        double traceLength = values[0];
        double traceStart = values[1];
        double fold = juce::jlimit(0.0f, 1.0f, values[2].load());

		const double safeFrequency = juce::jmax(1.0, (double)frequency);
		const int cycleSamples = juce::jlimit(1, (int)buffer.size(), (int)std::ceil(sampleRate / safeFrequency));
        const double trueCycleSamples = sampleRate / frequency;
        
		const double framePhase = (double)samplesSinceFrameStart / trueCycleSamples; // [0, 1)

        // Make tracePhase a biased triangle wave where the peak is at (1 - 0.5 * fold)
        // i.e. a saw wave at fold = 0, and a triangle wave at fold = 1
        // See p2(t) in https://www.desmos.com/calculator/ktfqptaidh
        double tracePhase = framePhase;
        if (fold > 0.0) {
            tracePhase = std::min(framePhase / (1.0 - 0.5 * fold),
                                  (framePhase - 1.0) / (-0.5 * fold));
        }

        tracePhase = traceStart + traceLength * tracePhase;
        tracePhase = tracePhase - std::floor(tracePhase); // Wrap in [0, 1)

        // The basic idea is to use the most recent COMPLETED cycle in the backbuffer
        // as a lookup table indexed by outFramePhase
		// Sample the backbuffer with linear interpolation instead of nearest
        // for extra juiciness
        double samplePos = bufferIndex - samplesSinceFrameStart + (tracePhase - 1) * trueCycleSamples - 1; // extra sample delay for good measure
		samplePos = samplePos - buffer.size() * std::floor(samplePos / (double)buffer.size()); // Wrap to [0, size)
		int lowIndex = (int)std::floor(samplePos) % buffer.size();
		int highIndex = (lowIndex + 1) % buffer.size();
		double mixFactor = samplePos - std::floor(samplePos); // Fractional part
		osci::Point output = (1 - mixFactor) * buffer[lowIndex] + mixFactor * buffer[highIndex];

		bufferIndex++;
		if (bufferIndex >= buffer.size()) {
			bufferIndex = 0;
		}
        // Fallback for contexts where we don't get explicit frame sync pulses
        samplesSinceFrameStart++;
        if (samplesSinceFrameStart >= cycleSamples) {
            samplesSinceFrameStart = 0;
        }

		return output;
	}

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
			std::make_shared<TraceEffect>(),
			std::vector<osci::EffectParameter*>{
                new osci::EffectParameter(
                    "Trace Length",
                    "Defines how much of the frame is drawn per cycle. This has the effect of 'tracing' out the image from a single dot when animated. By default, we draw the whole frame, corresponding to a value of 1.0.",
                    "traceLength",
                    VERSION_HINT, 0.5, 0.0, 3.0
                ),
				new osci::EffectParameter(
					"Trace Start",
					"Defines how far into the frame the drawing is started at. This has the effect of 'tracing' out the image from a single dot when animated. By default, we start drawing from the beginning of the frame, so this value is 0.0.",
					"traceStart",
					VERSION_HINT, 0.0, 0.0, 1.0, 0.0001, osci::LfoType::Sawtooth, 0.5
				),
                new osci::EffectParameter(
                    "Fold",
                    "With folding enabled, the trace position smoothly ramps back down to the minimum after reaching the maximum instead of instantly jumping to the minimum.",
                    "traceFold",
                    VERSION_HINT, 1.0, 0.0, 1.0
                ),
			}
		);
		eff->setName("Trace");
        return configureBuiltEffect(eff);
	}

private:
	std::vector<osci::Point> buffer;
	int bufferIndex = 0;
	int samplesSinceFrameStart = 0;
};
