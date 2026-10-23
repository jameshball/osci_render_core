#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

class ScaleEffectApp : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<ScaleEffectApp>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::scale(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<ScaleEffectApp>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Scale X", "Scales the object in the horizontal direction.", "scaleX", VERSION_HINT, 1.2, -3.0, 3.0),
                new osci::EffectParameter("Scale Y", "Scales the object in the vertical direction.", "scaleY", VERSION_HINT, 1.2, -3.0, 3.0),
                new osci::EffectParameter("Scale Z", "Scales the depth of the object.", "scaleZ", VERSION_HINT, 1.2, -3.0, 3.0),
            }
        );
        eff->setName("Scale");
        eff->markLockable(true);
        return configureBuiltEffect(eff);
    }
};
