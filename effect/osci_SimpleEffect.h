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

    SimpleEffect(const std::vector<EffectParameter*>& parameters) : SimpleEffect([](int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) {return input;}, parameters) {}

    SimpleEffect(EffectParameter* parameter) : SimpleEffect([](int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) {return input;}, parameter) {}


	void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const bool hasX = numChannels >= 1; // ch0 -> X
        const bool hasY = numChannels >= 2; // ch1 -> Y
        const bool hasZ = numChannels >= 3; // ch2 -> Z

        const bool useFunction = application != nullptr;
        const bool useClass = effectApplication != nullptr;
        
        // Get the source for animated values (either from our parent if we're a clone, or ourselves)
        const Effect* valueSource = animatedValuesSource ? animatedValuesSource : this;
        
        // Check if pre-animated values exist - if not, log warning once and use static values as fallback
        const bool hasPreAnimatedValues = valueSource->hasAnimatedValuesForBlock(static_cast<size_t>(numSamples));
        if (!hasPreAnimatedValues) {
            DBG("Warning: Effect '" + getId() + "' is missing pre-animated values! Using static parameter values as fallback.");
            // Cache static parameter values once (fallback path)
            for (size_t p = 0; p < parameters.size(); p++) {
                actualValues[p] = parameters[p]->getValueUnnormalised();
            }
        }

        for (int i = 0; i < numSamples; i++) {
            if (useClass && frameSyncInput != nullptr && i < frameSyncInput->getNumSamples()) {
                const float sync = frameSyncInput->getSample(0, i);
                if (sync > 0.5f) {
                    effectApplication->onFrameStart();
                }
            }

            // Copy pre-computed values from source to actualValues
            if (hasPreAnimatedValues) {
                for (size_t p = 0; p < parameters.size(); p++) {
                    actualValues[p] = valueSource->getAnimatedValue(p, static_cast<size_t>(i));
                }
            }
            // Note: if no pre-animated values, actualValues already set to static values above
            
            // Get frequency from frequency buffer (defaults to 220Hz if not provided)
            float frequency = 220.0f;
            if (frequencyInput != nullptr && i < frequencyInput->getNumSamples()) {
                frequency = frequencyInput->getSample(0, i);
            }

            const float x = hasX ? buffer.getSample(0, i) : 0.0f;
            const float y = hasY ? buffer.getSample(1, i) : 0.0f;
            const float z = hasZ ? buffer.getSample(2, i) : 0.0f;

            Point point(x, y, z);

            if (useFunction) {
                point = application(i, point, actualValues, sampleRate, frequency);
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

                    point = effectApplication->apply(i, point, externalPoint, actualValues, sampleRate, frequency);
                } else {
                    point = effectApplication->apply(i, point, Point(), actualValues, sampleRate, frequency);
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

    // Creates a clone of this effect that shares the same parameters but has fresh
    // application state. Used for per-voice effect processing.
    // The clone will read pre-animated values from the source effect.
    std::shared_ptr<SimpleEffect> cloneWithSharedParameters() const {
        std::shared_ptr<SimpleEffect> cloned;
        if (effectApplication != nullptr) {
            cloned = std::make_shared<SimpleEffect>(effectApplication->clone(), parameters);
        } else if (application != nullptr) {
            cloned = std::make_shared<SimpleEffect>(application, parameters);
        } else {
            cloned = std::make_shared<SimpleEffect>(parameters);
        }
        // Share the enabled/selected/linked pointers
        cloned->enabled = enabled;
        cloned->selected = selected;
        cloned->linked = linked;
        // Set this effect as the source for pre-animated values
        cloned->animatedValuesSource = this;
        // Copy other metadata
        if (name.has_value()) {
            cloned->setName(name.value());
        }
        cloned->setIcon(icon);
        cloned->setPrecedence(precedence);
        cloned->setPremiumOnly(premiumOnly);
        return cloned;
    }

private:
	EffectApplicationType application;
	std::shared_ptr<EffectApplication> effectApplication;
    // Pointer to the source effect that has pre-computed animated values.
    // Used by cloned effects to read the global pre-animated values.
    // IMPORTANT: The source effect must outlive this cloned effect.
    // In practice, this is the global effect in toggleableEffects which
    // persists for the lifetime of the processor.
    const Effect* animatedValuesSource = nullptr;
};

} // namespace osci
