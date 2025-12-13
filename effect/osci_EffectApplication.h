#pragma once
#include "../shape/osci_Point.h"
#include <JuceHeader.h>
#include <memory>

#define VERSION_HINT 2

namespace osci {

class Effect; // forward declaration to avoid include cycle

class EffectApplication {
public:
	EffectApplication() {};
	virtual ~EffectApplication() = default;

	virtual Point apply(int index, Point input, Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) = 0;
	// Factory to build a configured Effect wrapper for this application.
	// Implementations should construct a new Effect wrapping a new instance of the concrete EffectApplication
	// and populate it with the appropriate parameters (names, ids, ranges, defaults, lfo presets, icons, etc.).
	virtual std::shared_ptr<Effect> build() const = 0;
	// Creates a new instance of this EffectApplication with fresh state but same configuration.
	// Used for per-voice effect cloning where each voice needs independent state.
	virtual std::shared_ptr<EffectApplication> clone() const = 0;
	
	void resetPhase();
	double nextPhase(double frequency, double sampleRate);
private:
	double phase = 0.0;
};

} // namespace osci
