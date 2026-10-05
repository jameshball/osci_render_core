#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

class BitCrushEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<BitCrushEffect>();
	}

	// algorithm from https://www.kvraudio.com/forum/viewtopic.php?t=163880
	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		double effectScale = juce::jlimit(0.0f, 1.0f, values[0].load());
		const auto output = osci::point_effects::bitCrush(input, values[1].load());
		return ((1 - effectScale) * input + effectScale * output).withColour(input.r, input.g, input.b);
	}

	std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<BitCrushEffect>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Bit Crush",
                                          "Limits the resolution of points drawn to the screen, making the object look pixelated, and making the audio sound more 'digital' and distorted.",
                                          "bitCrushDryWet", VERSION_HINT, 1.0, 0.0, 1.0),
                new osci::EffectParameter("Bit Crush Strength",
                                          "Constrains the resolution which points are limited to.",
                                          "bitCrush",VERSION_HINT, 0.7, 0.0, 1.0)
        });
		return configureBuiltEffect(eff);
	}
};
