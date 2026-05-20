#pragma once

#include "../effect/osci_EffectApplication.h"
#include "../effect/osci_EffectParameter.h"
#include "../effect/osci_SimpleEffect.h"

namespace osci {

class StereoEffect : public EffectApplication {
public:
    std::shared_ptr<EffectApplication> clone() const override {
        auto cloned = std::make_shared<StereoEffect>();
        cloned->setIcon(getIcon());
        return cloned;
    }

    void prepareToPlay(float sr) override {
        sampleRate = sr;
        initialiseBuffer(sr);
    }

    Point apply(int index, Point input, Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
        juce::ignoreUnused(index, externalInput, sampleRate, frequency);

        if (buffer.empty()) {
            return input;
        }

        double sampleOffset = values[0].load() / 10.0;
        sampleOffset = juce::jlimit(0.0, 1.0, sampleOffset);
        sampleOffset *= static_cast<double>(buffer.size());

        head++;
        if (head >= static_cast<int>(buffer.size())) {
            head = 0;
        }

        buffer[static_cast<size_t>(head)] = input;

        int readHead = head - static_cast<int>(sampleOffset);
        if (readHead < 0) {
            readHead += static_cast<int>(buffer.size());
        }

        return Point(input.x, buffer[static_cast<size_t>(readHead)].y, input.z, input.r, input.g, input.b);
    }

    std::shared_ptr<Effect> build() const override {
        return configureBuiltEffect(std::make_shared<SimpleEffect>(
            std::make_shared<StereoEffect>(),
            new EffectParameter(
                "Stereo",
                "Turns mono audio that is uninteresting to visualise into stereo audio that is interesting to visualise.",
                "stereo",
                VERSION_HINT, 0.0, 0.0, 1.0
            )
        ));
    }

private:
    void initialiseBuffer(double sr) {
        buffer.clear();
        buffer.resize(static_cast<size_t>(bufferLength * sr));
        head = 0;
    }

    const double bufferLength = 0.1;
    double sampleRate = -1.0;
    std::vector<Point> buffer;
    int head = 0;
};

} // namespace osci
