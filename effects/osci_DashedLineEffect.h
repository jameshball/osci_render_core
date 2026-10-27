#pragma once
#include "../effect/osci_SimpleEffect.h"

#include <cmath>

class DashedLineEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<DashedLineEffect>();
	}

	void prepareToPlay(float sampleRate) override {
		buffer.resize((int)std::ceil(sampleRate));
		bufferIndex = 0;
		samplesSinceFrameStart = 0;
	}

	void onFrameStart() override {
		bufferIndex = 0;
		samplesSinceFrameStart = 0;
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		if (buffer.empty()) return input;

		double dashCount = juce::jmax(1.0f, values[0].load()); // Dashes per cycle
		double dashCoverage = juce::jlimit(0.0f, 1.0f, values[1].load());
        double dashOffset = values[2].load();

		const double safeFrequency = juce::jmax(1.0, (double)frequency);
		const int cycleSamples = juce::jlimit(1, (int)buffer.size(), (int)std::ceil(sampleRate / safeFrequency));

		// Fallback for contexts where we don't get explicit frame sync pulses
		if (samplesSinceFrameStart >= cycleSamples) {
			samplesSinceFrameStart = 0;
			bufferIndex = 0;
		}

		const double framePhase01 = (double)samplesSinceFrameStart / (double)cycleSamples; // [0, 1)
		const double dashLengthSamples = ((double)cycleSamples / dashCount);
		double dashPhase = framePhase01 * dashCount - dashOffset;
		dashPhase = dashPhase - std::floor(dashPhase); // Wrap

		buffer[bufferIndex] = input;

		// Linear interpolation works much better than nearest for this
		double samplePos = bufferIndex - dashLengthSamples * dashPhase * (1.0 - dashCoverage);
		samplePos = samplePos - cycleSamples * std::floor(samplePos / (double)cycleSamples); // Wrap to [0, cycleSamples)
		int lowIndex = (int)std::floor(samplePos) % cycleSamples;
		int highIndex = (lowIndex + 1) % cycleSamples;
		double mixFactor = samplePos - std::floor(samplePos); // Fractional part
		osci::Point output = (1 - mixFactor) * buffer[lowIndex] + mixFactor * buffer[highIndex];

		bufferIndex++;
		if (bufferIndex >= cycleSamples) {
			bufferIndex = 0;
		}
		samplesSinceFrameStart++;

		return output;
	}

	std::shared_ptr<osci::Effect> build() const override {
		auto eff = std::make_shared<osci::SimpleEffect>(
			std::make_shared<DashedLineEffect>(),
			std::vector<osci::EffectParameter*>{
				new osci::EffectParameter("Dash Count", "Controls the number of dashed lines in the drawing.", "dashCount", VERSION_HINT, 16.0, 1.0, 32.0),
				new osci::EffectParameter("Dash Width", "Controls how much each dash unit is drawn.", "dashWidth", VERSION_HINT, 0.5, 0.0, 1.0),
                new osci::EffectParameter("Dash Offset", "Offsets the location of the dashed lines.", "dashOffset", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001f, osci::LfoType::Sawtooth, 1.0f),
			}
		);
		eff->setName("Dash");
		return configureBuiltEffect(eff);
	}

private:
	std::vector<osci::Point> buffer;
	int bufferIndex = 0;
	int samplesSinceFrameStart = 0;
};
