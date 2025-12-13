#include "osci_Effect.h"
#include <numbers>
#include <cmath>

namespace osci {

void Effect::animateValues(int numSamples, const juce::AudioBuffer<float>* volumeBuffer) {
    const size_t numParameters = parameters.size();
    const size_t blockSize = static_cast<size_t>(numSamples);
    const float sr = static_cast<float>(sampleRate);
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float pi = juce::MathConstants<float>::pi;
    
    // Resize buffer if needed (only resize if too small to avoid allocations)
    if (animatedValuesBuffer.size() != numParameters) {
        animatedValuesBuffer.resize(numParameters);
    }
    for (size_t i = 0; i < numParameters; i++) {
        if (animatedValuesBuffer[i].size() < blockSize) {
            animatedValuesBuffer[i].resize(blockSize);
        }
    }
    
    // Process each parameter as a complete block
    for (size_t paramIdx = 0; paramIdx < numParameters; paramIdx++) {
        auto* param = parameters[paramIdx];
        float* outBuffer = animatedValuesBuffer[paramIdx].data();
        
        const float minValue = param->min;
        const float maxValue = param->max;
        const float range = maxValue - minValue;
        
        // Determine LFO type once per block (not per sample!)
        const bool lfoEnabled = param->isLfoEnabled();
        const LfoType lfoType = lfoEnabled ? static_cast<LfoType>(static_cast<int>(param->lfo->getValueUnnormalised())) : LfoType::Static;
        
        // Check sidechain once per block
        const bool useSidechain = param->sidechain != nullptr && param->sidechain->getBoolValue();
        
        if (lfoType == LfoType::Static) {
            // ===== STATIC PATH: No LFO, just smoothing towards target =====
            
            // Compute smoothing weight once per block
            const float smoothRaw = static_cast<float>(param->smoothValueChange);
            const bool instantSmoothing = smoothRaw >= 1.0f;
            float smoothingWeight = 1.0f;
            if (!instantSmoothing) {
                float svc = juce::jlimit(SMOOTHING_SPEED_MIN, 1.0f, smoothRaw);
                smoothingWeight = svc * (192000.0f / sr) * 0.001f;
            }
            
            // Get target value (constant for non-sidechain)
            const float staticTarget = useSidechain ? 0.0f : param->getValueUnnormalised();
            
            // Current smoothed value (carried from previous block)
            float current = actualValues[paramIdx].load();
            
            // Process all samples in block
            for (size_t i = 0; i < blockSize; i++) {
                float target;
                if (useSidechain) {
                    float volume = (volumeBuffer != nullptr && static_cast<int>(i) < volumeBuffer->getNumSamples()) 
                        ? volumeBuffer->getSample(0, static_cast<int>(i)) : 1.0f;
                    target = volume * range + minValue;
                } else {
                    target = staticTarget;
                }
                
                if (instantSmoothing) {
                    current = target;
                } else {
                    const float diff = std::abs(current - target);
                    if (diff < EFFECT_SNAP_THRESHOLD) {
                        current = target;
                    } else {
                        current = std::fma(smoothingWeight, (target - current), current);
                    }
                }
                outBuffer[i] = current;
            }
            
            // Store final value for next block
            actualValues[paramIdx] = current;
            
        } else {
            // ===== LFO PATH =====
            
            // Compute LFO bounds once per block
            float lfoMin = minValue;
            float lfoMax = maxValue;
            if (lfoEnabled) {
                const float startNorm = (param->lfoStartPercent != nullptr) 
                    ? juce::jlimit(0.0f, 1.0f, param->lfoStartPercent->getValueUnnormalised() / 100.0f) : 0.0f;
                const float endNorm = (param->lfoEndPercent != nullptr) 
                    ? juce::jlimit(0.0f, 1.0f, param->lfoEndPercent->getValueUnnormalised() / 100.0f) : 1.0f;
                lfoMin = minValue + startNorm * range;
                lfoMax = minValue + endNorm * range;
            }
            const float lfoRange = lfoMax - lfoMin;
            
            // Compute phase increment once per block
            const float rate = (param->lfoRate != nullptr) ? param->lfoRate->getValueUnnormalised() : 0.0f;
            const float phaseInc = (sr > 0.0f) ? (rate / sr) : 0.0f;
            
            // Get current phase (carried from previous block)
            float phase = param->phase;
            
            // RNG state for noise (carried from previous block)
            uint32_t rngState = param->rngState;
            
            // Process all samples based on LFO type
            switch (lfoType) {
                case LfoType::Sine: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        const float s = std::sin(phase * twoPi - pi) * 0.5f + 0.5f;
                        outBuffer[i] = std::fma(s, lfoRange, lfoMin);
                    }
                    break;
                }
                case LfoType::Square: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        outBuffer[i] = (phase < 0.5f) ? lfoMax : lfoMin;
                    }
                    break;
                }
                case LfoType::Seesaw: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        float tri = (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
                        float x = juce::jlimit(0.0f, 1.0f, tri);
                        float soft = x * x * (3.0f - 2.0f * x); // smoothstep
                        outBuffer[i] = std::fma(soft, lfoRange, lfoMin);
                    }
                    break;
                }
                case LfoType::Triangle: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        float tri = (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
                        outBuffer[i] = std::fma(tri, lfoRange, lfoMin);
                    }
                    break;
                }
                case LfoType::Sawtooth: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        outBuffer[i] = std::fma(phase, lfoRange, lfoMin);
                    }
                    break;
                }
                case LfoType::ReverseSawtooth: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        outBuffer[i] = std::fma(1.0f - phase, lfoRange, lfoMin);
                    }
                    break;
                }
                case LfoType::Noise: {
                    for (size_t i = 0; i < blockSize; i++) {
                        phase += phaseInc;
                        if (phase >= 1.0f) phase -= 1.0f;
                        // xorshift32 PRNG
                        rngState ^= rngState << 13;
                        rngState ^= rngState >> 17;
                        rngState ^= rngState << 5;
                        const float rnd = (rngState & 0x00FFFFFFu) * (1.0f / 16777215.0f);
                        outBuffer[i] = std::fma(rnd, lfoRange, lfoMin);
                    }
                    break;
                }
                default: {
                    // Fallback: just use parameter value
                    const float val = param->getValueUnnormalised();
                    for (size_t i = 0; i < blockSize; i++) {
                        outBuffer[i] = val;
                    }
                    break;
                }
            }
            
            // Store final phase and RNG state for next block
            param->phase = phase;
            param->rngState = rngState;
            
            // Store final value for actualValues (last sample of block)
            actualValues[paramIdx] = outBuffer[blockSize - 1];
        }
    }
}

float Effect::getValue(int index) {
	return parameters[index]->getValueUnnormalised();
}

float Effect::getValue() {
	return getValue(0);
}

float Effect::getActualValue(int index) {
    return actualValues[index];
}

float Effect::getActualValue() {
	return actualValues[0];
}

void Effect::setValue(int index, float value) {
	parameters[index]->setUnnormalisedValueNotifyingHost(value);
}

void Effect::setValue(float value) {
	setValue(0, value);
}

int Effect::getPrecedence() {
	return precedence;
}

void Effect::setPrecedence(int precedence) {
	this->precedence = precedence;
}

void Effect::addListener(int index, juce::AudioProcessorParameter::Listener* listener) {
	parameters[index]->addListener(listener);
	if (parameters[index]->lfo != nullptr) {
		parameters[index]->lfo->addListener(listener);
	}
	if (parameters[index]->lfoRate != nullptr) {
		parameters[index]->lfoRate->addListener(listener);
	}
	if (enabled != nullptr) {
		enabled->addListener(listener);
	}
	if (selected != nullptr) {
		selected->addListener(listener);
	}
	if (linked != nullptr) {
		linked->addListener(listener);
	}
	if (parameters[index]->sidechain != nullptr) {
        parameters[index]->sidechain->addListener(listener);
    }
}

void Effect::removeListener(int index, juce::AudioProcessorParameter::Listener* listener) {
	if (parameters[index]->sidechain != nullptr) {
        parameters[index]->sidechain->removeListener(listener);
    }
	if (linked != nullptr) {
		linked->removeListener(listener);
	}
	if (enabled != nullptr) {
		enabled->removeListener(listener);
	}
	if (selected != nullptr) {
		selected->removeListener(listener);
	}
	if (parameters[index]->lfoRate != nullptr) {
		parameters[index]->lfoRate->removeListener(listener);
	}
	if (parameters[index]->lfo != nullptr) {
		parameters[index]->lfo->removeListener(listener);
	}
    parameters[index]->removeListener(listener);
}

void Effect::markEnableable(bool enable) {
	if (enabled != nullptr) {
        enabled->setValue(enable);
	} else {
		enabled = new BooleanParameter(getName() + " Enabled", getId() + "Enabled", parameters[0]->getVersionHint(), enable, "Toggles the effect.");
	}
}

void Effect::markLockable(bool lock) {
	if (linked != nullptr) {
		linked->setValue(lock);
	}
	else {
		linked = new BooleanParameter(getName() + " Locked", getId() + "Locked", parameters[0]->getVersionHint(), lock, "Locks each parameter in the effect to have the same value.");
	}
}

void Effect::markSelectable(bool select) {
	if (selected != nullptr) {
		selected->setValue(select);
	} else {
		// Default to true for backwards compatibility when created
		selected = new BooleanParameter(getName() + " Selected", getId() + "Selected", parameters[0]->getVersionHint(), select, "Marks the effect as present/selected in the chain.");
	}
}

juce::String Effect::getId() {
	return parameters[0]->paramID;
}

const juce::String Effect::getName() const {
	return name.value_or(parameters[0]->getName(9999));
}

void Effect::setPremiumOnly(bool premium) {
    premiumOnly = premium;
}

bool Effect::isPremiumOnly() const {
    return premiumOnly;
}

void Effect::save(juce::XmlElement* xml) {
	if (enabled != nullptr) {
		auto enabledXml = xml->createNewChildElement("enabled");
		enabled->save(enabledXml);
	}
	if (selected != nullptr) {
		auto selectedXml = xml->createNewChildElement("selected");
		selected->save(selectedXml);
	}
	if (linked != nullptr) {
		auto lockedXml = xml->createNewChildElement("locked");
		linked->save(lockedXml);
	}
	xml->setAttribute("id", getId());
	xml->setAttribute("precedence", precedence);
	for (auto parameter : parameters) {
		parameter->save(xml->createNewChildElement("parameter"));
	}
}

void Effect::load(juce::XmlElement* xml) {
	if (enabled != nullptr) {
		auto enabledXml = xml->getChildByName("enabled");
        if (enabledXml != nullptr) {
            enabled->load(enabledXml);
        }
	}
	if (selected != nullptr) {
		auto selectedXml = xml->getChildByName("selected");
		if (selectedXml != nullptr) {
			selected->load(selectedXml);
		} else {
			// Backwards compatibility: default selected to true if missing
			selected->setBoolValueNotifyingHost(true);
		}
	}
	if (linked != nullptr) {
		auto lockedXml = xml->getChildByName("locked");
        if (lockedXml != nullptr) {
            linked->load(lockedXml);
        }
	}
    if (xml->hasAttribute("precedence")) {
        setPrecedence(xml->getIntAttribute("precedence"));
    }
    for (auto parameterXml : xml->getChildIterator()) {
        auto parameter = getParameter(parameterXml->getStringAttribute("id"));
        if (parameter != nullptr) {
            parameter->load(parameterXml);
        }
    }
}

EffectParameter* Effect::getParameter(juce::String id) {
	for (auto parameter : parameters) {
        if (parameter->paramID == id) {
            return parameter;
        }
    }
    return nullptr;
}

void Effect::resetToDefault() {
	if (linked != nullptr)
		linked->setValueNotifyingHost(linked->getDefaultValue());

	for (auto* p : parameters) {
		if (p == nullptr) continue;
		p->resetToDefault(true);
	}

	// Recompute actual values to reflect the newly reset parameter values
	animateValues(1, nullptr);
}

} // namespace osci
