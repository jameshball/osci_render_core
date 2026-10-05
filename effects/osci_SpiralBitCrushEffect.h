#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

class SpiralBitCrushEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<SpiralBitCrushEffect>();
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>&values, float sampleRate, float frequency) override {
		double effectScale = juce::jlimit(0.0f, 1.0f, values[0].load());
		const auto output = osci::point_effects::spiralCrush(input, values[1].load(), values[2].load(), values[3].load(), values[4].load());
		return ((1 - effectScale) * input + effectScale * output).withColour(input.r, input.g, input.b);
	}

	std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<SpiralBitCrushEffect>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Spiral Bit Crush",
                                          "Constrains points to a spiral pattern.",
                                          "spiralBitCrush", VERSION_HINT, 0.4, 0.0, 1.0),
                new osci::EffectParameter("Spiral Density",
                                          "Controls the density of the spiral pattern.",
                                          "spiralBitCrushDensity", VERSION_HINT, 13.0, 3.0, 30.0, 1.0),
                new osci::EffectParameter("Spiral Twist",
                                          "Controls how much the spiral pattern twists.",
                                          "spiralBitCrushTwist", VERSION_HINT, 0.6, -1.0, 1.0),
                new osci::EffectParameter("Zoom",
                                          "Zooms the spiral pattern.",
                                          "spiralBitCrushZoom", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001, osci::LfoType::Sawtooth, 0.1),
                new osci::EffectParameter("Rotation",
                                          "Rotates the spiral pattern.",
                                          "spiralBitCrushRotation", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001, osci::LfoType::ReverseSawtooth, 0.02)
            }
        );
		return configureBuiltEffect(eff);
	}
};
