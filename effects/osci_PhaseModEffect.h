#pragma once
#include "../effect/osci_SimpleEffect.h"

#include <cmath>

class PhaseModEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<PhaseModEffect>();
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

        // TODO: Experiment with timbre variations (e.g. bend control, which is
        // currently commented out)
        
        // If I don't lock coarse freq to a whole number multiple of base freq,
        // there is no way to guarantee initial phase offset on note trigger
        // So force whole number coarse freq and make phase offset a parameter
        double pmAmp = values[0];
        double pmCoarseFrequency = juce::jmax(1.0f, values[1].load());
        //double bendAmount = 8.0 * values[2];
        double pmPhaseOffset = values[2];

		const double safeFrequency = juce::jmax(1.0, (double)frequency);
		const int cycleSamples = juce::jlimit(1, (int)buffer.size(), (int)std::ceil(sampleRate / safeFrequency));
        const double trueCycleSamples = sampleRate / frequency;
        
		const double framePhase = (double)samplesSinceFrameStart / trueCycleSamples; // [0, 1)
        double pmPhase = (framePhase * pmCoarseFrequency - pmPhaseOffset);
        /*
        pmPhase = pmPhase - std::floor(pmPhase); // Wrap to [0, 1) to apply bend
        if (bendAmount != 0.0) {
            pmPhase = (1.0 - std::exp(bendAmount * pmPhase)) / (1.0 - std::exp(bendAmount));
        }
        */
        double pmSignal = pmAmp * std::sin(pmPhase * juce::MathConstants<double>::twoPi)
                         / pmCoarseFrequency / juce::MathConstants<double>::twoPi;
        double outFramePhase = framePhase + pmSignal;
        outFramePhase = outFramePhase - std::floor(outFramePhase); // Wrap in [0, 1)

        // The basic idea is to use the most recent COMPLETED cycle in the backbuffer
        // as a lookup table indexed by outFramePhase
		// Sample the backbuffer with linear interpolation instead of nearest
        // for extra juiciness
        double samplePos = bufferIndex - samplesSinceFrameStart + (outFramePhase - 1) * trueCycleSamples - 1; // extra sample delay for good measure
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
			std::make_shared<PhaseModEffect>(),
			std::vector<osci::EffectParameter*>{
				new osci::EffectParameter("Phase Modulation", "Applies phase modulation to the sound.", "pmAmp", VERSION_HINT, 0.0, 0.0, 30.0),
				new osci::EffectParameter("Coarse Frequency", "Multiplies the frequency of the phase modulation oscillator.", "pmCoarseFrequency", VERSION_HINT, 1.0, 1.0, 48.0, 1.0),
                //new osci::EffectParameter("Bend", "Bends the phase modulation wave resulting in thinner, sharper timbres.", "pmBend", VERSION_HINT, 0.0, 0.0, 1.0),
                new osci::EffectParameter("Phase Offset", "Controls the phase offset of the phase modulation oscillator.", "pmPhaseOffset", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001f, osci::LfoType::Sawtooth, 0.5f),
			}
		);
		eff->setName("Phase Modulation");
		return configureBuiltEffect(eff);
	}

private:
	std::vector<osci::Point> buffer;
	int bufferIndex = 0;
	int samplesSinceFrameStart = 0;
};
