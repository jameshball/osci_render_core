#pragma once
#include "../shape/osci_Point.h"
#include <JuceHeader.h>

#define SMOOTHING_SPEED_CONSTANT 0.3
#define SMOOTHING_SPEED_MIN 0.00001

namespace osci {

class BooleanParameter : public juce::AudioProcessorParameterWithID {
public:
	BooleanParameter(juce::String name, juce::String id, int versionHint, bool value, juce::String description)
		: AudioProcessorParameterWithID(juce::ParameterID(id, versionHint), name), value(value), description(description), defaultValue(value) {}

	juce::String getName(int maximumStringLength) const override {
		return name.substring(0, maximumStringLength);
	}

	juce::String getLabel() const override {
        return juce::String();
    }

	float getValue() const override {
		return value.load();
	}

	bool getBoolValue() const {
        return value.load();
    }

	void setValue(float newValue) override {
		value.store(newValue >= 0.5f);
    }

	void setBoolValue(bool newValue) {
		value.store(newValue);
	}

	void setBoolValueNotifyingHost(bool newValue) {
        setValueNotifyingHost(newValue ? 1.0f : 0.0f);
    }

	float getDefaultValue() const override {
        return defaultValue.load() ? 1.0f : 0.0f;
    }

	int getNumSteps() const override {
		return 2;
    }

	bool isDiscrete() const override {
        return true;
    }

	bool isBoolean() const override {
        return true;
    }

	bool isOrientationInverted() const override {
        return false;
    }

	juce::String getText(float value, int maximumStringLength) const override {
		juce::String string = value ? "true" : "false";
		return string.substring(0, maximumStringLength);
	}

	float getValueForText(const juce::String& text) const override {
		return text.length() > 0 && text.toLowerCase()[0] == 't';
    }

	bool isAutomatable() const override {
        return true;
    }

	bool isMetaParameter() const override {
        return false;
    }

	juce::AudioProcessorParameter::Category getCategory() const override {
        return juce::AudioProcessorParameter::genericParameter;
    }

	void save(juce::XmlElement* xml) {
		xml->setAttribute("id", paramID);
		xml->setAttribute("value", value.load());
    }

	void load(juce::XmlElement* xml) {
		setBoolValueNotifyingHost(xml->getBoolAttribute("value", getDefaultValue()));
    }
    
    juce::String getDescription() {
        return description;
    }

	// Reset current value to default
	void resetToDefault(bool notifyHost = true) {
		if (notifyHost) {
			setBoolValueNotifyingHost(defaultValue.load());
		} else {
			setBoolValue(defaultValue.load());
		}
	}

private:
	std::atomic<bool> value = false;
	std::atomic<bool> defaultValue = false;
    juce::String description;
};

class FloatParameter : public juce::AudioProcessorParameterWithID {
public:
	std::atomic<float> min = 0.0;
	std::atomic<float> max = 0.0;
	std::atomic<float> step = 0.0;
	// Defaults for bounds
	std::atomic<float> defaultMin = 0.0f;
	std::atomic<float> defaultMax = 0.0f;
    
    std::atomic<float> defaultValue = 0.0;

    FloatParameter(juce::String name, juce::String id, int versionHint, float value, float min, float max, float step = 0.69, juce::String label = "") : juce::AudioProcessorParameterWithID(juce::ParameterID(id, versionHint), name), step(step), value(value), label(label), defaultValue(value) {
		// need to initialise here because of naming conflicts on Windows
		this->min = min;
		this->max = max;
		// capture defaults
		this->defaultMin = min;
		this->defaultMax = max;
	}

	juce::String getName(int maximumStringLength) const override {
		return name.substring(0, maximumStringLength);
	}

	juce::String getLabel() const override {
		return label;
	}

	// returns value in 
	// [0, 1]
	float getNormalisedValue(float value) const {
		// clip value to valid range
		auto min = this->min.load();
		auto max = this->max.load();
		value = juce::jlimit(min, max, value);
		// normalize value to range [0, 1]
		return (value - min) / (max - min);
	}

	float getUnnormalisedValue(float value) const {
		value = juce::jlimit(0.0f, 1.0f, value);
		auto min = this->min.load();
		auto max = this->max.load();
		return min + value * (max - min);
	}

	float getValue() const override {
		return getNormalisedValue(value.load());
	}

	float getValueUnnormalised() const {
		return value.load();
	}

	void setValue(float newValue) override {
		value = getUnnormalisedValue(newValue);
	}

	void setValueUnnormalised(float newValue) {
		value = newValue;
	}

	void setUnnormalisedValueNotifyingHost(float newValue) {
		setValueNotifyingHost(getNormalisedValue(newValue));
	}

	float getDefaultValue() const override {
        return getNormalisedValue(defaultValue.load());
	}

	int getNumSteps() const override {
		return (max.load() - min.load()) / step.load();
	}

	bool isDiscrete() const override {
		return false;
	}

	bool isBoolean() const override {
		return false;
	}

	bool isOrientationInverted() const override {
		return false;
	}

	juce::String getText(float value, int maximumStringLength) const override {
		auto string = juce::String(getUnnormalisedValue(value), 3);
		return string.substring(0, maximumStringLength);
	}

	float getValueForText(const juce::String& text) const override {
		return getNormalisedValue(text.getFloatValue());
	}

	bool isAutomatable() const override {
		return true;
	}

	bool isMetaParameter() const override {
		return false;
	}

	juce::AudioProcessorParameter::Category getCategory() const override {
		return juce::AudioProcessorParameter::genericParameter;
	}

	void save(juce::XmlElement* xml) {
		xml->setAttribute("id", paramID);
		xml->setAttribute("value", value.load());
		xml->setAttribute("min", min.load());
		xml->setAttribute("max", max.load());
		xml->setAttribute("step", step.load());
	}

	// opt to not change any values if not found
	void load(juce::XmlElement* xml) {
        if (xml->hasAttribute("min")) {
            min = xml->getDoubleAttribute("min");
        }
        if (xml->hasAttribute("max")) {
            max = xml->getDoubleAttribute("max");
        }
        if (xml->hasAttribute("step")) {
            step = xml->getDoubleAttribute("step");
        }
		if (xml->hasAttribute("value")) {
			setUnnormalisedValueNotifyingHost(xml->getDoubleAttribute("value"));
		}
    }

	// Reset value and range to defaults
	virtual void resetToDefault(bool notifyHost = true) {
		if (notifyHost) {
			setUnnormalisedValueNotifyingHost(defaultValue.load());
		} else {
			setValueUnnormalised(defaultValue.load());
		}
		min = defaultMin.load();
		max = defaultMax.load();
	}

private:
	// value is not necessarily in the range [min, max] so effect applications may need to clip to a valid range
	std::atomic<float> value = 0.0;
	juce::String label;
};

class IntParameter : public juce::AudioProcessorParameterWithID {
public:
	std::atomic<int> min = 0;
	std::atomic<int> max = 10;
	// Defaults for bounds
	std::atomic<int> defaultMin = 0;
	std::atomic<int> defaultMax = 10;
    
    std::atomic<int> defaultValue = 0;

    IntParameter(juce::String name, juce::String id, int versionHint, int value, int min, int max) : AudioProcessorParameterWithID(juce::ParameterID(id, versionHint), name), value(value), defaultValue(value) {
		// need to initialise here because of naming conflicts on Windows
		this->min = min;
		this->max = max;
		// capture defaults
		this->defaultMin = min;
		this->defaultMax = max;
	}

	juce::String getName(int maximumStringLength) const override {
		return name.substring(0, maximumStringLength);
	}

	juce::String getLabel() const override {
		return juce::String();
	}

	// returns value in range [0, 1]
	float getNormalisedValue(float value) const {
		// clip value to valid range
		auto min = this->min.load();
		auto max = this->max.load();
		value = juce::jlimit(min, max, (int) value);
		// normalize value to range [0, 1]
		return (value - min) / (max - min);
	}

	float getUnnormalisedValue(float value) const {
		value = juce::jlimit(0.0f, 1.0f, value);
		auto min = this->min.load();
		auto max = this->max.load();
		return min + value * (max - min);
	}

	float getValue() const override {
		return getNormalisedValue(value.load());
	}

	float getValueUnnormalised() const {
		return value.load();
	}

	void setValue(float newValue) override {
		value = getUnnormalisedValue(newValue);
	}

	void setValueUnnormalised(float newValue) {
		value = newValue;
	}

	void setUnnormalisedValueNotifyingHost(float newValue) {
		setValueNotifyingHost(getNormalisedValue(newValue));
	}

	float getDefaultValue() const override {
        return getNormalisedValue(defaultValue.load());
	}

	int getNumSteps() const override {
		return max.load() - min.load() + 1;
	}

	bool isDiscrete() const override {
		return true;
	}

	bool isBoolean() const override {
		return false;
	}

	bool isOrientationInverted() const override {
		return false;
	}

	juce::String getText(float value, int maximumStringLength) const override {
		auto string = juce::String((int) getUnnormalisedValue(value));
		return string.substring(0, maximumStringLength);
	}

	float getValueForText(const juce::String& text) const override {
		return getNormalisedValue(text.getIntValue());
	}

	bool isAutomatable() const override {
		return true;
	}

	bool isMetaParameter() const override {
		return false;
	}

	juce::AudioProcessorParameter::Category getCategory() const override {
		return juce::AudioProcessorParameter::genericParameter;
	}

	void save(juce::XmlElement* xml) {
        xml->setAttribute("id", paramID);
        xml->setAttribute("value", value.load());
        xml->setAttribute("min", min.load());
        xml->setAttribute("max", max.load());
    }

	// opt to not change any values if not found
	void load(juce::XmlElement* xml) {
		if (xml->hasAttribute("min")) {
            min = xml->getIntAttribute("min");
        }
		if (xml->hasAttribute("max")) {
            max = xml->getIntAttribute("max");
        }
		if (xml->hasAttribute("value")) {
			setUnnormalisedValueNotifyingHost(xml->getIntAttribute("value"));
		}
    }

	// Reset value and range to defaults
	virtual void resetToDefault(bool notifyHost = true) {
		if (notifyHost) {
			setUnnormalisedValueNotifyingHost(defaultValue.load());
		} else {
			setValueUnnormalised(defaultValue.load());
		}
		min = defaultMin.load();
		max = defaultMax.load();
	}

private:
	// value is not necessarily in the range [min, max] so effect applications may need to clip to a valid range
	std::atomic<int> value = 0;
};

enum class LfoType : int {
	Static = 1,
	Sine = 2,
	Square = 3,
	Seesaw = 4,
	Triangle = 5,
	Sawtooth = 6,
	ReverseSawtooth = 7,
	Noise = 8
};

class LfoTypeParameter : public IntParameter {
public:
	LfoTypeParameter(juce::String name, juce::String id, int versionHint, int value) : IntParameter(name, id, versionHint, value, 1, 8) {}

	juce::String getText(float value, int maximumStringLength) const override {
		switch ((LfoType)(int)getUnnormalisedValue(value)) {
            case LfoType::Static:
                return "Static";
            case LfoType::Sine:
                return "Sine";
            case LfoType::Square:
                return "Square";
            case LfoType::Seesaw:
                return "Seesaw";
            case LfoType::Triangle:
                return "Triangle";
            case LfoType::Sawtooth:
                return "Sawtooth";
            case LfoType::ReverseSawtooth:
                return "Reverse Sawtooth";
            case LfoType::Noise:
                return "Noise";
            default:
                return "Unknown";
        }
	}

	float getValueForText(const juce::String& text) const override {
		int unnormalisedValue;
		if (text == "Static") {
			unnormalisedValue = (int)LfoType::Static;
		} else if (text == "Sine") {
			unnormalisedValue = (int)LfoType::Sine;
		} else if (text == "Square") {
			unnormalisedValue = (int)LfoType::Square;
		} else if (text == "Seesaw") {
			unnormalisedValue = (int)LfoType::Seesaw;
		} else if (text == "Triangle") {
			unnormalisedValue = (int)LfoType::Triangle;
		} else if (text == "Sawtooth") {
			unnormalisedValue = (int)LfoType::Sawtooth;
		} else if (text == "Reverse Sawtooth") {
			unnormalisedValue = (int)LfoType::ReverseSawtooth;
		} else if (text == "Noise") {
			unnormalisedValue = (int)LfoType::Noise;
		} else {
			unnormalisedValue = (int)LfoType::Static;
        }
		return getNormalisedValue(unnormalisedValue);
	}

	void save(juce::XmlElement* xml) {
        xml->setAttribute("lfo", getText(getValue(), 100));
    }

	void load(juce::XmlElement* xml) {
        setValueNotifyingHost(getValueForText(xml->getStringAttribute("lfo")));
    }
};

class EffectParameter : public FloatParameter {
public:
	std::atomic<double> smoothValueChange = SMOOTHING_SPEED_CONSTANT;
	LfoTypeParameter* lfo = nullptr;
	FloatParameter* lfoRate = nullptr;
    FloatParameter* lfoStartPercent = nullptr;
    FloatParameter* lfoEndPercent = nullptr;
	BooleanParameter* sidechain = nullptr;
	std::atomic<float> phase = 0.0f;
	juce::String description;

	std::vector<juce::AudioProcessorParameter*> getParameters() {
		std::vector<juce::AudioProcessorParameter*> parameters;
		parameters.push_back(this);
		if (lfo != nullptr) {
			parameters.push_back(lfo);
		}
		if (lfoRate != nullptr) {
			parameters.push_back(lfoRate);
		}
        if (lfoStartPercent != nullptr) {
            parameters.push_back(lfoStartPercent);
        }
        if (lfoEndPercent != nullptr) {
            parameters.push_back(lfoEndPercent);
        }
		if (sidechain != nullptr) {
			parameters.push_back(sidechain);
		}
		return parameters;
    }

	void disableLfo() {
        lfoEnabled = false;
		delete lfo;
		delete lfoRate;
        delete lfoStartPercent;
        delete lfoEndPercent;
		lfo = nullptr;
		lfoRate = nullptr;
        lfoStartPercent = nullptr;
        lfoEndPercent = nullptr;
	}

	void disableSidechain() {
        delete sidechain;
        sidechain = nullptr;
    }

	void save(juce::XmlElement* xml) {
		FloatParameter::save(xml);
		xml->setAttribute("smoothValueChange", smoothValueChange.load());

		if (lfoEnabled) {
			auto lfoXml = xml->createNewChildElement("lfo");
			lfo->save(lfoXml);
			lfoRate->save(lfoXml);
            auto lfoStartXml = xml->createNewChildElement("lfoStart");
            lfoStartPercent->save(lfoStartXml);
            auto lfoEndXml = xml->createNewChildElement("lfoEnd");
            lfoEndPercent->save(lfoEndXml);
		}

		if (sidechain != nullptr) {
			auto sidechainXml = xml->createNewChildElement("sidechain");
			sidechain->save(sidechainXml);
		}
    }

	void load(juce::XmlElement* xml) {
        FloatParameter::load(xml);
		if (xml->hasAttribute("smoothValueChange")) {
			smoothValueChange = xml->getDoubleAttribute("smoothValueChange");
        } else {
            smoothValueChange = SMOOTHING_SPEED_CONSTANT;
        }

		if (lfoEnabled) {
			auto lfoXml = xml->getChildByName("lfo");
			if (lfoXml != nullptr) {
				lfo->load(lfoXml);
				lfoRate->load(lfoXml);
			} else {
				lfo->setUnnormalisedValueNotifyingHost(lfo->defaultValue.load());
				lfoRate->setUnnormalisedValueNotifyingHost(lfoRate->defaultValue.load());
			}
            
            auto lfoStartXml = xml->getChildByName("lfoStart");
			if (lfoStartXml != nullptr) {
				lfoStartPercent->load(lfoStartXml);
			} else {
				lfoStartPercent->setUnnormalisedValueNotifyingHost(lfoStartPercent->defaultValue.load());
			}
            
            auto lfoEndXml = xml->getChildByName("lfoEnd");
			if (lfoEndXml != nullptr) {
				lfoEndPercent->load(lfoEndXml);
			} else {
				lfoEndPercent->setUnnormalisedValueNotifyingHost(lfoEndPercent->defaultValue.load());
			}
		}

		if (sidechain != nullptr) {
			auto sidechainXml = xml->getChildByName("sidechain");
			if (sidechainXml != nullptr) {
				sidechain->load(sidechainXml);
			} else {
				sidechain->setBoolValueNotifyingHost(false);
			}
		}
    }
    
    bool isLfoEnabled() {
        return lfoEnabled;
    }

	EffectParameter(juce::String name, juce::String description, juce::String id, int versionHint, float value, float min, float max, float step = 0.0001, LfoType lfoDefault = LfoType::Static, float lfoRateDefault = 1.0f) : FloatParameter(name, id, versionHint, value, min, max, step), description(description) {
		// Create modulation parameters and sidechain controls with provided defaults
		lfo = new LfoTypeParameter(name + " LFO", id + "Lfo", versionHint, (int) lfoDefault);
		lfoRate = new FloatParameter(name + " LFO Rate", id + "LfoRate", versionHint, lfoRateDefault, 0.0f, 10000.0f, 0.001f, "Hz");
		lfoStartPercent = new FloatParameter(name + " LFO Start", id + "LfoStart", versionHint, 0.0f, 0.0f, 100.0f, 0.0001f, "%");
		lfoEndPercent = new FloatParameter(name + " LFO End", id + "LfoEnd", versionHint, 100.0f, 0.0f, 100.0f, 0.0001f, "%");
		sidechain = new BooleanParameter(name + " Sidechain Enabled", id + "Sidechain", versionHint, false, "Toggles " + name + " Sidechain.");
	}

	// Reset this parameter (value, range) and all associated modulation/sidechain to defaults
	void resetToDefault(bool notifyHost = true) override {
		// Reset base (value + range)
		FloatParameter::resetToDefault(notifyHost);
		// Reset smoothing
		smoothValueChange = SMOOTHING_SPEED_CONSTANT;
		// Reset modulation params
		if (lfo != nullptr) lfo->resetToDefault(notifyHost);
		if (lfoRate != nullptr) lfoRate->resetToDefault(notifyHost);
		if (lfoStartPercent != nullptr) lfoStartPercent->resetToDefault(notifyHost);
		if (lfoEndPercent != nullptr) lfoEndPercent->resetToDefault(notifyHost);
		// Reset sidechain
		if (sidechain != nullptr) sidechain->resetToDefault(notifyHost);
		// Reset phase
		phase = 0.0f;
	}
    
private:
    bool lfoEnabled = true;
};

} // namespace osci