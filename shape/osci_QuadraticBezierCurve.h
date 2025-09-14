#pragma once

#include "osci_Shape.h"
#include "osci_CubicBezierCurve.h"

namespace osci {
class QuadraticBezierCurve : public CubicBezierCurve {
public:
	QuadraticBezierCurve(float x1, float y1, float x2, float y2, float x3, float y3);
};
} // namespace osci