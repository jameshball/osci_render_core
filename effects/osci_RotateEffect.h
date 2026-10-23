#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

#include <numbers>

class RotateEffectApp : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<RotateEffectApp>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::rotate(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<RotateEffectApp>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Rotate X", "Controls the rotation of the object in the X axis.", "rotateX", VERSION_HINT, 0.0, -1.0, 1.0),
                new osci::EffectParameter("Rotate Y", "Controls the rotation of the object in the Y axis.", "rotateY", VERSION_HINT, 0.0, -1.0, 1.0, 0.0001f, osci::LfoType::Sawtooth, 0.2f),
                new osci::EffectParameter("Rotate Z", "Controls the rotation of the object in the Z axis.", "rotateZ", VERSION_HINT, 0.0, -1.0, 1.0),
            }
        );
        eff->setName("Rotate");
        return configureBuiltEffect(eff);
    }
};
