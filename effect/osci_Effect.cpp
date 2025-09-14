#include "osci_Effect.h"
#include <numbers>
#include <cmath>

namespace osci {

void Effect::animateValues(float volume) {
	const size_t numParameters = parameters.size();
	const float sr = (float) sampleRate;
	constexpr float twoPi = juce::MathConstants<float>::twoPi;

	for (size_t i = 0; i < numParameters; i++) {
		auto* parameter = parameters[i];
		float minValue = parameter->min;
		float maxValue = parameter->max;
		const float rangeOrig = maxValue - minValue;

		// Non-LFO (default) path early-out: smoothing and/or sidechain only
		const int lfoTypeInt = parameter->isLfoEnabled() ? (int) parameter->lfo->getValueUnnormalised() : (int) LfoType::Static;
		if (lfoTypeInt == (int) LfoType::Static) {
			// Refresh cached smoothing weight if control changed since last cache
			const float smoothRaw = (float) parameter->smoothValueChange;
			if (smoothRaw != parameter->lastSmoothValueChange) {
				float svc = juce::jlimit(SMOOTHING_SPEED_MIN, 1.0f, smoothRaw);
				// (value/1000) * 192000 / sampleRate
				parameter->cachedSmoothingWeight = svc * (192000.0f / sr) * 0.001f;
				parameter->lastSmoothValueChange = smoothRaw;
			}

			const float weight = parameter->cachedSmoothingWeight;
			float newValue;
			if (parameter->sidechain != nullptr && parameter->sidechain->getBoolValue()) {
				newValue = (float) volume * rangeOrig + minValue;
			} else {
				newValue = parameter->getValueUnnormalised();
			}
			if (smoothRaw >= 1.0f) {
				actualValues[i] = newValue;
			} else {
				const float cur = (float) actualValues[i];
				const float diff = std::abs(cur - newValue);
				if (diff < EFFECT_SNAP_THRESHOLD) {
					actualValues[i] = newValue;
				} else {
					actualValues[i] = std::fma(weight, (newValue - cur), cur);
				}
			}
			continue;
		}

		// LFO path
		const LfoType type = (LfoType) lfoTypeInt;
		// Maintain normalized phase in [0,1); nextPhase returns normalized phase
		const float percentage = nextPhase(parameter);

		// Update cached bounds normalization only if control changed; remember if a change occurred
		bool lfoStartChanged = false;
		bool lfoEndChanged = false;
		if (parameter->lfoStartPercent != nullptr) {
			float raw = parameter->lfoStartPercent->getValueUnnormalised();
			if (raw != parameter->lastLfoStartRaw) {
				parameter->lfoStartNorm = juce::jlimit(0.0f, 1.0f, raw / 100.0f);
				parameter->lastLfoStartRaw = raw;
				lfoStartChanged = true;
			}
		}
		if (parameter->lfoEndPercent != nullptr) {
			float raw = parameter->lfoEndPercent->getValueUnnormalised();
			if (raw != parameter->lastLfoEndRaw) {
				parameter->lfoEndNorm = juce::jlimit(0.0f, 1.0f, raw / 100.0f);
				parameter->lastLfoEndRaw = raw;
				lfoEndChanged = true;
			}
		}

		// Apply LFO bounds mapping with caching of computed bounds
		if (parameter->isLfoEnabled()) {
			const float originalMin = minValue;
			const float originalMax = maxValue;
			const bool boundsChanged = (originalMin != parameter->lastParamMin) || (originalMax != parameter->lastParamMax);
			if (boundsChanged) {
				parameter->lastParamMin = originalMin;
				parameter->lastParamMax = originalMax;
			}
			if (boundsChanged || lfoStartChanged || lfoEndChanged || parameter->lastLfoStartRaw == OSCI_UNINITIALIZED_F || parameter->lastLfoEndRaw == OSCI_UNINITIALIZED_F) {
				const float lfoMin = originalMin + parameter->lfoStartNorm * (originalMax - originalMin);
				const float lfoMax = originalMin + parameter->lfoEndNorm * (originalMax - originalMin);
				parameter->cachedLfoMinBound = lfoMin;
				parameter->cachedLfoMaxBound = lfoMax;
			}
			minValue = parameter->cachedLfoMinBound;
			maxValue = parameter->cachedLfoMaxBound;
		}
		const float range = maxValue - minValue;

		float out = 0.0f;
		switch (type) {
			case LfoType::Sine: {
				const float s = juce::dsp::FastMathApproximations::sin(percentage * twoPi) * 0.5f + 0.5f;
				out = std::fma(s, range, minValue);
				break;
			}
			case LfoType::Square: {
				out = (percentage < 0.5f) ? maxValue : minValue;
				break;
			}
			case LfoType::Seesaw: {
				float tri = (percentage < 0.5f) ? (percentage * 2.0f) : ((1.0f - percentage) * 2.0f);
				// Cheap smoothstep-like curve instead of exp sigmoid
				float x = juce::jlimit(0.0f, 1.0f, tri);
				float soft = x * x * (3.0f - 2.0f * x); // smoothstep
				out = std::fma(soft, range, minValue);
				break;
			}
			case LfoType::Triangle: {
				float tri = (percentage < 0.5f) ? (percentage * 2.0f) : ((1.0f - percentage) * 2.0f);
				out = std::fma(tri, range, minValue);
				break;
			}
			case LfoType::Sawtooth: {
				out = std::fma(percentage, range, minValue);
				break;
			}
			case LfoType::ReverseSawtooth: {
				out = std::fma(1.0f - percentage, range, minValue);
				break;
			}
			case LfoType::Noise: {
				// xorshift32 PRNG per parameter
				uint32_t state = parameter->rngState;
				state ^= state << 13;
				state ^= state >> 17;
				state ^= state << 5;
				parameter->rngState = state;
				const float rnd = (state & 0x00FFFFFFu) * (1.0f / 16777215.0f);
				out = std::fma(rnd, range, minValue);
				break;
			}
			default: {
				out = parameter->getValueUnnormalised();
				break;
			}
		}
		actualValues[i] = out;
	}
}

// should only be the audio thread calling this, but either way it's not a big deal
float Effect::nextPhase(EffectParameter* p) {
	// Update phase increment cache if rate changed
	const float sr = (float) sampleRate;
	const float rate = p->lfoRate != nullptr ? p->lfoRate->getValueUnnormalised() : 0.0f;
	if (rate != p->lastLfoRate) {
		p->phaseInc = (sr > 0.0f ? (rate / sr) : 0.0f);
		p->lastLfoRate = rate;
	}

	float ph = p->phase + p->phaseInc;
	if (ph >= 1.0f) ph -= 1.0f;
	p->phase = ph;
	return ph; // normalized [0,1)
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
	animateValues(0.0);
}

} // namespace osci
