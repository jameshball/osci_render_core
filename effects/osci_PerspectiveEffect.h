#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

class PerspectiveEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<PerspectiveEffect>();
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		auto effectScale = values[0].load();
		const auto projected = osci::point_effects::perspective(input, values[1].load());
		return osci::Point((1 - effectScale) * input.x + effectScale * projected.x, (1 - effectScale) * input.y + effectScale * projected.y, 0).withColour(input.r, input.g, input.b);
	}

	std::shared_ptr<osci::Effect> build() const override {
		auto eff = std::make_shared<osci::SimpleEffect>(
			std::make_shared<PerspectiveEffect>(),
			std::vector<osci::EffectParameter*>{
				new osci::EffectParameter("Perspective", "Controls the strength of the 3D perspective projection.", "perspectiveStrength", VERSION_HINT, 1.0, 0.0, 1.0),
				new osci::EffectParameter("Field of View", "Controls the camera's field of view in degrees. A lower field of view makes the image look more flat, and a higher field of view makes the image look more 3D.", "perspectiveFov", VERSION_HINT, 50.0, 5.0, 130.0),
			}
		);
		return configureBuiltEffect(eff);
	}
};
