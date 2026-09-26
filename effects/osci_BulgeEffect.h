#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

class BulgeEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<BulgeEffect>();
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::bulge(input, values[0].load());
    }

	std::shared_ptr<osci::Effect> build() const override {
		auto eff = std::make_shared<osci::SimpleEffect>(
			std::make_shared<BulgeEffect>(),
			new osci::EffectParameter("Bulge", "Applies a bulge that makes the centre of the image larger, and squishes the edges of the image. This applies a distortion to the audio.", "bulge", VERSION_HINT, 0.5, 0.0, 1.0));
		return configureBuiltEffect(eff);
	}
};
