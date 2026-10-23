#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <numbers>

class TwistEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<TwistEffect>();
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>&values, float sampleRate, float frequency) override {
		return osci::point_effects::twist(input, values[0].load());
	}
    
    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<TwistEffect>(),
            new osci::EffectParameter("Twist", "Twists the image in a corkscrew pattern.", "twist", VERSION_HINT, 0.5, 0.0, 1.0, 0.0001, osci::LfoType::Sine, 0.5)
        );
        return configureBuiltEffect(eff);
    }
};
