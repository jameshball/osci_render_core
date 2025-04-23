#include "osci_EffectApplication.h"
#include <numbers>
#include "../osci_Util.h"

namespace osci {

void EffectApplication::resetPhase() {
	phase = 0.0;
}

double EffectApplication::nextPhase(double frequency, double sampleRate) {
    phase += 2 * std::numbers::pi * frequency / sampleRate;
    phase = Util::wrapAngle(phase);

    return phase;
}

} // namespace osci