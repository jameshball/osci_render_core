#include "osci_EffectApplication.h"
#include "osci_Effect.h"
#include <numbers>
#include "../osci_Util.h"

namespace osci {

EffectApplication& EffectApplication::withIcon(const juce::String& newIcon) {
    setIcon(newIcon);
    return *this;
}

void EffectApplication::setIcon(const juce::String& newIcon) {
    icon = newIcon;
}

const juce::String& EffectApplication::getIcon() const {
    return icon;
}

std::shared_ptr<Effect> EffectApplication::configureBuiltEffect(std::shared_ptr<Effect> effect) const {
    if (effect != nullptr && icon.isNotEmpty()) {
        effect->setIcon(icon);
    }
    return effect;
}

void EffectApplication::resetPhase() {
	phase = -std::numbers::pi;
}

double EffectApplication::nextPhase(double frequency, double sampleRate) {
    phase += 2 * std::numbers::pi * frequency / sampleRate;
    phase = Util::wrapAngle(phase);

    return phase;
}

} // namespace osci
