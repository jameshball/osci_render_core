#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <numbers>

class RippleEffectApp : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<RippleEffectApp>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::ripple(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<RippleEffectApp>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Ripple Depth", "Controls how large the ripples applied to the image are.", "rippleDepth", VERSION_HINT, 0.2, 0.0, 1.0),
                new osci::EffectParameter("Ripple Phase", "Controls the position of the ripple. Animate this to see a moving ripple effect.", "ripplePhase", VERSION_HINT, 0.0, -1.0, 1.0, 0.0001f, osci::LfoType::Sawtooth, 1.0f),
                new osci::EffectParameter("Ripple Amount", "Controls how many ripples are applied to the image.", "rippleAmount", VERSION_HINT, 0.1, 0.0, 1.0),
            }
        );
        eff->setName("Ripple");
        return configureBuiltEffect(eff);
    }
};
