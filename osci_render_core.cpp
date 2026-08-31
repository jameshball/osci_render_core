#include "osci_render_core.h"

// Include settings implementations

// Include effect implementations
#include "effect/osci_Effect.cpp"
#include "effect/osci_EffectApplication.cpp"

// Include shape implementations
#include "shape/osci_Shape.cpp"
#include "shape/osci_Point.cpp"
#include "shape/osci_Line.cpp"
#include "shape/osci_CircleArc.cpp"
#include "shape/osci_CubicBezierCurve.cpp"
#include "shape/osci_QuadraticBezierCurve.cpp"

// Include geometry implementations
#include "geometry/osci_PerspectiveProjector.cpp"

// Include midi implementations
#include "midi/osci_MidiManager.cpp"

// Include concurrency implementations
#include "concurrency/osci_AudioBackgroundThread.cpp"
#include "concurrency/osci_AudioBackgroundThreadManager.cpp"

// Include DSP implementations
#include "dsp/osci_IntegerRatioSampleRateAdapter.cpp"

namespace osci
{
    // The base class is pure virtual, so no implementation is needed here
} // namespace osci
