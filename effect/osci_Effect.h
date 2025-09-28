#pragma once
#include "../shape/osci_Point.h"
#include <JuceHeader.h>
#include "osci_EffectApplication.h"
#include "osci_EffectParameter.h"

namespace osci {



class ProcessorBase : public juce::AudioProcessor
{
public:
    //==============================================================================
    ProcessorBase()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo()).withOutput ("Output", juce::AudioChannelSet::stereo()))
    {
    }
    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioSampleBuffer&, juce::MidiBuffer&) override {}
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    //==============================================================================
    const juce::String getName() const override { return {}; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    //==============================================================================
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

typedef std::function<Point(int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate)> EffectApplicationType;

class Effect : public ProcessorBase {
public:
	// AudioProcessor overrides
	const juce::String getName() const override;
    void prepareToPlay(double sr, int samplesPerBlock) override {
        sampleRate = (float) sr;
        // Recompute caches that depend on sample rate for all parameters
        for (auto* p : parameters) {
            if (p == nullptr) continue;
            // Phase increment cache
            const float lfoRateHz = p->lfoRate != nullptr ? p->lfoRate->getValueUnnormalised() : 0.0f;
            p->phaseInc = (sampleRate > 0.0f ? (lfoRateHz / sampleRate) : 0.0f);
            p->lastLfoRate = lfoRateHz;

            // Smoothing weight cache: clamp and map from ms-ish units to per-sample weight
            float svc = (float) juce::jlimit(SMOOTHING_SPEED_MIN, 1.0f, p->smoothValueChange.load());
            svc *= (192000.0f / sampleRate) * 0.001f; // (value/1000) * 192000 / sr
            p->cachedSmoothingWeight = svc;
            p->lastSmoothValueChange = p->smoothValueChange;

            // LFO start/end percent normalization cache
            if (p->lfoStartPercent != nullptr) {
                float raw = p->lfoStartPercent->getValueUnnormalised();
                p->lfoStartNorm = juce::jlimit(0.0f, 1.0f, raw / 100.0f);
                p->lastLfoStartRaw = raw;
            }
            if (p->lfoEndPercent != nullptr) {
                float raw = p->lfoEndPercent->getValueUnnormalised();
                p->lfoEndNorm = juce::jlimit(0.0f, 1.0f, raw / 100.0f);
                p->lastLfoEndRaw = raw;
            }

            // Initialize cached LFO mapped bounds based on current parameter bounds
            const float pMin = p->min.load();
            const float pMax = p->max.load();
            p->lastParamMin = pMin;
            p->lastParamMax = pMax;
            const float lfoMin = pMin + p->lfoStartNorm * (pMax - pMin);
            const float lfoMax = pMin + p->lfoEndNorm * (pMax - pMin);
            p->cachedLfoMinBound = lfoMin;
            p->cachedLfoMaxBound = lfoMax;
        }
    }
	virtual void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override = 0;

	virtual std::vector<EffectParameter*> initialiseParameters() const = 0;

	float getValue(int index);
	float getValue();
	float getActualValue(int index);
	float getActualValue();
	void setValue(int index, float value);
	void setValue(float value);
	int getPrecedence();
	void setPrecedence(int precedence);
	void addListener(int index, juce::AudioProcessorParameter::Listener* listener);
	void removeListener(int index, juce::AudioProcessorParameter::Listener* listener);
	void markEnableable(bool enabled);
    void markLockable(bool lock);
	void markSelectable(bool select);
	juce::String getId();

    void setPremiumOnly(bool premium);
    bool isPremiumOnly() const;

	void save(juce::XmlElement* xml);
	void load(juce::XmlElement* xml);
	EffectParameter* getParameter(juce::String id);

	// Reset all parameters for this effect back to their default values
	void resetToDefault();

	std::vector<EffectParameter*> parameters;
    BooleanParameter* enabled = nullptr;
    BooleanParameter* linked = nullptr;
	BooleanParameter* selected = nullptr; // whether this effect is present/selected in the list
    
    void setName(const juce::String& newName) {
        name = newName;
    }
    
    void setIcon(const juce::String& newIcon) {
        icon = newIcon;
    }
    
    juce::String getIcon() {
        return icon;
    }

	inline void setExternalInput(juce::AudioBuffer<float>* buffer) {
		externalInput = buffer;
	}

protected:
	
    std::optional<juce::String> name;
    juce::String icon = "";
    
	juce::SpinLock listenerLock;
    std::vector<std::atomic<float>> actualValues;
	int precedence = -1;
    float sampleRate = 192000;

    bool premiumOnly = false;

	juce::AudioBuffer<float>* externalInput = nullptr;

    void animateValues(float volume);
    // Returns normalized phase in [0,1)
    float nextPhase(EffectParameter* parameter);
};

} // namespace osci
