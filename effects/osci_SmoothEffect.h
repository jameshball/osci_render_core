#pragma once

#include "../effect/osci_EffectApplication.h"
#include "../effect/osci_EffectParameter.h"
#include "../effect/osci_SimpleEffect.h"

#include <cmath>

namespace osci {

class SmoothEffect : public EffectApplication {
public:
    SmoothEffect() = default;
    explicit SmoothEffect(juce::String prefix, float defaultValue = 0.75f) : idPrefix(prefix), smoothingDefault(defaultValue) {}

    std::shared_ptr<EffectApplication> clone() const override {
        auto cloned = std::make_shared<SmoothEffect>(idPrefix, smoothingDefault);
        cloned->setIcon(getIcon());
        return cloned;
    }

    Point apply(int index, Point input, Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        juce::ignoreUnused(index, externalInput, frequency);

        float weight = juce::jmax(values[0].load(), 0.00001f);
        weight *= 0.95f;

        constexpr float strength = 10.0f;
        weight = std::log(strength * weight + 1.0f) / std::log(strength + 1.0f);

        const float sampleRateScale = 48000.0f / juce::jmax(sampleRate, 1.0f);
        weight = std::pow(weight, sampleRateScale);
        avg = weight * avg + (1.0f - weight) * input;

        return avg;
    }

    std::shared_ptr<Effect> build() const override {
        auto id = idPrefix.isEmpty() ? juce::String("smoothing") : (idPrefix + "Smoothing");
        return configureBuiltEffect(std::make_shared<SimpleEffect>(
            std::make_shared<SmoothEffect>(id),
            new EffectParameter("Smoothing", "This works as a low-pass frequency filter that removes high frequencies, making the image look smoother, and audio sound less harsh.", id, VERSION_HINT, smoothingDefault, 0.0, 1.0)
        ));
    }

private:
    Point avg;
    juce::String idPrefix;
    float smoothingDefault = 0.5f;
};

} // namespace osci
