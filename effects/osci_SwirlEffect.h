#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

class SwirlEffectApp : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<SwirlEffectApp>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::swirl(input, values[0].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<SwirlEffectApp>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Swirl", "Swirls the image in a spiral pattern.", "swirl", VERSION_HINT, 0.4, -1.0, 1.0),
            }
        );
        return configureBuiltEffect(eff);
    }
};
