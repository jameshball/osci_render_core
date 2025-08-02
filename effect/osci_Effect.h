#pragma once
#include "../shape/osci_Point.h"
#include <JuceHeader.h>
#include "osci_EffectApplication.h"
#include "osci_EffectParameter.h"

namespace osci {

typedef std::function<Point(int index, Point input, const std::vector<std::atomic<double>>& values, double sampleRate)> EffectApplicationType;

class Effect {
public:
	Effect(std::shared_ptr<EffectApplication> effectApplication, const std::vector<EffectParameter*>& parameters);
	Effect(std::shared_ptr<EffectApplication> effectApplication, EffectParameter* parameter);
	Effect(EffectApplicationType application, const std::vector<EffectParameter*>& parameters);
	Effect(EffectApplicationType application, EffectParameter* parameter);
    Effect(const std::vector<EffectParameter*>& parameters);
    Effect(EffectParameter* parameter);

    Point apply(int index, Point input, double volume = 0.0, bool animate = true);
	
	void apply();
	double getValue(int index);
	double getValue();
	double getActualValue(int index);
	double getActualValue();
	void setValue(int index, double value);
	void setValue(double value);
	int getPrecedence();
	void setPrecedence(int precedence);
	void addListener(int index, juce::AudioProcessorParameter::Listener* listener);
	void removeListener(int index, juce::AudioProcessorParameter::Listener* listener);
	void markEnableable(bool enabled);
    void markLockable(bool lock);
	juce::String getId();
	juce::String getName();
	void save(juce::XmlElement* xml);
	void load(juce::XmlElement* xml);
	EffectParameter* getParameter(juce::String id);
    void updateSampleRate(int sampleRate);

	std::vector<EffectParameter*> parameters;
    BooleanParameter* enabled = nullptr;
    BooleanParameter* linked = nullptr;

	void setExternalInput(Point externalInput) {
		if (effectApplication != nullptr) {
			effectApplication->extInput = externalInput;
		}
	}
    
    void setName(const juce::String& newName) {
        name = newName;
    }
    
    void setIcon(const juce::String& newIcon) {
        icon = newIcon;
    }
    
    juce::String getIcon() {
        return icon;
    }

private:
	
    std::optional<juce::String> name;
    juce::String icon = "";
    
	juce::SpinLock listenerLock;
    std::vector<std::atomic<double>> actualValues;
	int precedence = -1;
	std::atomic<int> sampleRate = 192000;
	EffectApplicationType application;
	
	std::shared_ptr<EffectApplication> effectApplication;

	void animateValues(double volume);
	float nextPhase(EffectParameter* parameter);
};

} // namespace osci
