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

	virtual Point apply(int index, Point input, const std::vector<std::atomic<float>>& values, float sampleRate) = 0;
	Point extInput = { 0,0 };
	// Factory to build a configured Effect wrapper for this application.
	// Implementations should construct a new Effect wrapping a new instance of the concrete EffectApplication
	// and populate it with the appropriate parameters (names, ids, ranges, defaults, lfo presets, icons, etc.).
	virtual std::shared_ptr<Effect> build() const = 0;
	
	void resetPhase();
	double nextPhase(double frequency, double sampleRate);
private:
	double phase = 0.0;
};

} // namespace osci
