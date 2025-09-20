#pragma once
#include <JuceHeader.h>
#include "osci_Effect.h"

namespace osci {

class SimpleEffect : public Effect {
public:
	SimpleEffect(std::shared_ptr<EffectApplication> effectApplication, const std::vector<EffectParameter*>& parameters) : effectApplication(effectApplication) {
        this->parameters = parameters;
        this->actualValues = std::vector<std::atomic<float>>(parameters.size());
        for (int i = 0; i < parameters.size(); i++) {
            actualValues[i] = parameters[i]->getValueUnnormalised();
        }
    }

    SimpleEffect(std::shared_ptr<EffectApplication> effectApplication, EffectParameter* parameter) : SimpleEffect(effectApplication, std::vector<EffectParameter*>{parameter}) {}

    SimpleEffect(EffectApplicationType application, const std::vector<EffectParameter*>& parameters) : application(application) {
        this->parameters = parameters;
        this->actualValues = std::vector<std::atomic<float>>(parameters.size());
        for (int i = 0; i < parameters.size(); i++) {
            actualValues[i] = parameters[i]->getValueUnnormalised();
        }
    }

    SimpleEffect(EffectApplicationType application, EffectParameter* parameter) : SimpleEffect(application, std::vector<EffectParameter*>{parameter}) {}

    SimpleEffect(const std::vector<EffectParameter*>& parameters) : SimpleEffect([](int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate) {return input;}, parameters) {}

    SimpleEffect(EffectParameter* parameter) : SimpleEffect([](int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate) {return input;}, parameter) {}


	void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        const int numChannels = buffer.getNumChannels();
        const bool hasX = numChannels >= 1; // ch0 -> X
        const bool hasY = numChannels >= 2; // ch1 -> Y
        const bool hasZ = numChannels >= 3; // ch2 -> Z

        const bool useFunction = application != nullptr;
        const bool useClass = effectApplication != nullptr;

        for (int i = 0; i < buffer.getNumSamples(); i++) {
            animateValues(0.0);

            const float x = hasX ? buffer.getSample(0, i) : 0.0f;
            const float y = hasY ? buffer.getSample(1, i) : 0.0f;
            const float z = hasZ ? buffer.getSample(2, i) : 0.0f;

            Point point(x, y, z);

            if (useFunction) {
                point = application(i, point, actualValues, sampleRate);
            } else if (useClass) {
                if (externalInput != nullptr) {
                    Point externalPoint;
                    if (externalInput->getNumChannels() > 1) {
                        externalPoint.x = externalInput->getSample(0, i);
                        externalPoint.y = externalInput->getSample(1, i);
                    } else if (externalInput->getNumChannels() > 0) {
                        externalPoint.x = externalInput->getSample(0, i);
                        externalPoint.y = externalInput->getSample(0, i);
                    }

                    point = effectApplication->apply(i, point, externalPoint, actualValues, sampleRate);
                } else {
                    point = effectApplication->apply(i, point, Point(), actualValues, sampleRate);
                }
            }

            if (hasX) buffer.setSample(0, i, point.x);
            if (hasY) buffer.setSample(1, i, point.y);
            if (hasZ) buffer.setSample(2, i, point.z);
        }
    }

	std::vector<EffectParameter*> initialiseParameters() const override {
        return parameters;
    }
private:
	EffectApplicationType application;
	std::shared_ptr<EffectApplication> effectApplication;
};

} // namespace osci
