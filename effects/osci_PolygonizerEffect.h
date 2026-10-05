#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

// Inspired by xenontesla122
class PolygonizerEffect : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<PolygonizerEffect>();
    }

    osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>&values, float sampleRate, float frequency) override {
        double effectScale = juce::jlimit(0.0f, 1.0f, values[0].load());
        const auto output = osci::point_effects::polygon(input, values[1].load(), values[2].load(), values[3].load(), values[4].load());
        return ((1 - effectScale) * input + effectScale * output).withColour(input.r, input.g, input.b);
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<PolygonizerEffect>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Polygonizer",
                                          "Constrains points to a polygon pattern.",
                                          "polygonizer", VERSION_HINT, 1.0, 0.0, 1.0),
                new osci::EffectParameter("Sides", "Controls the number of sides of the polygon pattern.",
                                          "polygonizerSides", VERSION_HINT, 5.0, 3.0, 8.0),
                new osci::EffectParameter("Stripe Size",
                                          "Controls the spacing between the stripes of the polygon pattern.",
                                          "polygonizerStripeSize", VERSION_HINT, 0.5, 0.0, 1.0),
                new osci::EffectParameter("Rotation", "Rotates the polygon pattern.",
                                          "polygonizerRotation", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001, osci::LfoType::Sawtooth, 0.1),
                new osci::EffectParameter("Stripe Phase", "Offsets the stripes of the polygon pattern.",
                                          "polygonizerStripePhase", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001, osci::LfoType::Sawtooth, 2.0)
            }
        );
        return configureBuiltEffect(eff);
    }
};
