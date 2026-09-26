#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <cmath>

class VortexEffect : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<VortexEffect>();
    }

    osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>&values, float sampleRate, float frequency) override {
        return osci::point_effects::vortex(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<VortexEffect>(),
            std::vector<osci::EffectParameter *>{
                new osci::EffectParameter("Vortex Strength",
                                          "Controls the strength of the vortex effect.",
                                          "vortexStrength", VERSION_HINT, 0.6, 0.0, 1.0),
                new osci::EffectParameter("Vortex Amount",
                                          "The multiplier applied to each point's angle, creating more vortexes.",
                                          "vortexAmount", VERSION_HINT, 2.0, 2.0, 6.0, 1.0),
                new osci::EffectParameter("Vortex Rotation",
                                          "The rotation applied to each point.",
                                          "vortexRotation", VERSION_HINT, 0.25, 0.0, 1.0, 0.0001, osci::LfoType::Sawtooth, 0.2)
        });
        eff->setName("Vortex");
        return configureBuiltEffect(eff);
    }
};
