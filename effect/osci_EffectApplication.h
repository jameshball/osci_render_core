#pragma once
#include "../shape/osci_Point.h"
#include <JuceHeader.h>

namespace osci {

class EffectApplication {
public:
	EffectApplication() {};

	virtual Point apply(int index, Point input, const std::vector<std::atomic<double>>& values, double sampleRate) = 0;
	Point extInput = { 0,0 };
	
	void resetPhase();
	double nextPhase(double frequency, double sampleRate);
private:
	double phase = 0.0;
};

} // namespace osci
