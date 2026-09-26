#pragma once
#include "../effect/osci_SimpleEffect.h"
#include "osci_PointEffectKernels.h"

class TranslateEffectApp : public osci::EffectApplication {
public:
    std::shared_ptr<osci::EffectApplication> clone() const override {
        return std::make_shared<TranslateEffectApp>();
    }

    osci::Point apply(int /*index*/, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        return osci::point_effects::translate(input, values[0].load(), values[1].load(), values[2].load());
    }

    std::shared_ptr<osci::Effect> build() const override {
        auto eff = std::make_shared<osci::SimpleEffect>(
            std::make_shared<TranslateEffectApp>(),
            std::vector<osci::EffectParameter*>{
                new osci::EffectParameter("Translate X", "Moves the object horizontally.", "translateX", VERSION_HINT, 0.3, -1.0, 1.0),
                new osci::EffectParameter("Translate Y", "Moves the object vertically.", "translateY", VERSION_HINT, 0.0, -1.0, 1.0),
                new osci::EffectParameter("Translate Z", "Moves the object away from the camera.", "translateZ", VERSION_HINT, 0.0, -1.0, 1.0),
            }
        );
        eff->setName("Translate");
        return configureBuiltEffect(eff);
    }
};
