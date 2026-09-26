#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"
#include <numbers>

// Simple shear (skew) along each axis: X += skewX * Y, Y += skewY * Z, Z += skewZ * X
// Useful subtle perspective-like distortion that can be layered with scale/rotate.
class SkewEffect : public osci::EffectApplication {
public:
    SkewEffect() {}

    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<SkewEffect>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::skew(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<SkewEffect>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Skew X", "Skews (shears) the shape horizontally based on vertical position.", "skewX", VERSION_HINT, 0.0, -1.0, 1.0, 0.0001f, osci::LfoType::Sine, 0.2f),
                new osci::EffectParameter("Skew Y", "Skews (shears) the shape vertically based on depth.", "skewY", VERSION_HINT, 0.0, -1.0, 1.0),
                new osci::EffectParameter("Skew Z", "Skews (shears) the shape in depth based on horizontal position.", "skewZ", VERSION_HINT, 0.0, -1.0, 1.0),
            }
        );
        eff->setName("Skew");
        return configureBuiltEffect(eff);
    }
};
