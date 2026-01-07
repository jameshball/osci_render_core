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

typedef std::function<Point(int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency)> EffectApplicationType;

class Effect : public ProcessorBase {
public:
	// AudioProcessor overrides
	const juce::String getName() const override;
    void prepareToPlay(double sr, int samplesPerBlock) override {
        sampleRate = static_cast<float>(sr);
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

	inline void setVolumeInput(juce::AudioBuffer<float>* buffer) {
		volumeInput = buffer;
	}

	inline void setFrequencyInput(juce::AudioBuffer<float>* buffer) {
        frequencyInput = buffer;
    }

    inline void setFrameSyncInput(juce::AudioBuffer<float>* buffer) {
        frameSyncInput = buffer;
    }

    // Pre-compute animated values for an entire block. Call this once per block before
    // any voices process. The animated values can then be read via getAnimatedValue().
    void animateValues(int numSamples, const juce::AudioBuffer<float>* volumeBuffer);
    
    // Get pre-computed animated value for a parameter at a specific sample index.
    // Must call animateValues() first for the current block.
    inline float getAnimatedValue(size_t paramIndex, size_t sampleIndex) const {
        if (paramIndex < animatedValuesBuffer.size() && sampleIndex < animatedValuesBuffer[paramIndex].size()) {
            return animatedValuesBuffer[paramIndex][sampleIndex];
        }
        // Fallback to current actualValues if buffer not populated
        return paramIndex < actualValues.size() ? actualValues[paramIndex].load() : 0.0f;
    }
    
    // Check if animated values buffer is valid for the given sample count
    inline bool hasAnimatedValuesForBlock(size_t numSamples) const {
        return !animatedValuesBuffer.empty() && 
               !animatedValuesBuffer[0].empty() && 
               animatedValuesBuffer[0].size() >= numSamples;
    }

    // Convenience method that sets all inputs, processes the block, then clears all inputs
    inline void processBlockWithInputs(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                       juce::AudioBuffer<float>* externalInput,
                                       juce::AudioBuffer<float>* volumeInput,
                                                                             juce::AudioBuffer<float>* frequencyInput,
                                                                             juce::AudioBuffer<float>* frameSyncInput = nullptr) {
        setExternalInput(externalInput);
        setVolumeInput(volumeInput);
        setFrequencyInput(frequencyInput);
		setFrameSyncInput(frameSyncInput);
        processBlock(buffer, midiMessages);
        setExternalInput(nullptr);
        setVolumeInput(nullptr);
        setFrequencyInput(nullptr);
		setFrameSyncInput(nullptr);
    }

protected:
	
    std::optional<juce::String> name;
    juce::String icon = "";
    
	juce::SpinLock listenerLock;
    std::vector<std::atomic<float>> actualValues;
	int precedence = -1;
    float sampleRate = 192000;

    bool premiumOnly = false;

    juce::AudioBuffer<float>* frequencyInput = nullptr;
    juce::AudioBuffer<float>* frameSyncInput = nullptr;
	juce::AudioBuffer<float>* externalInput = nullptr;
	juce::AudioBuffer<float>* volumeInput = nullptr;

    // Pre-computed animated values buffer: [parameterIndex][sampleIndex]
    std::vector<std::vector<float>> animatedValuesBuffer;
};

} // namespace osci
