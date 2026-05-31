#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "../geometry/osci_PerspectiveProjector.h"

#include <cmath>

class PerspectiveEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<PerspectiveEffect>();
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		auto effectScale = values[0].load();
		// Far plane clipping happens at about 1.2 deg for 100 far plane dist
		float fovDegrees = juce::jlimit(1.5f, 179.0f, values[1].load());
		float fov = juce::degreesToRadians(fovDegrees);

		// Place camera such that field of view is tangent to unit sphere
		osci::Vec3 origin = osci::Vec3(0, 0, -1.0f / std::sin(0.5f * (float)fov));
		projector.setCameraPosition(origin);
		projector.setFieldOfViewRadians(fov);
		osci::Vec3 vec = osci::Vec3(input.x, input.y, input.z);

		osci::Vec3 projected = projector.project(vec);

			return osci::Point(
				(1 - effectScale) * input.x + effectScale * projected.x,
				(1 - effectScale) * input.y + effectScale * projected.y,
				0
			).withColour(input.r, input.g, input.b);
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

private:
	
	osci::PerspectiveProjector projector;
};
