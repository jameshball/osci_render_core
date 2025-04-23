#pragma once

#include "osci_Shape.h"
#include "osci_CubicBezierCurve.h"

namespace osci {
class QuadraticBezierCurve : public CubicBezierCurve {
public:
	QuadraticBezierCurve(double x1, double y1, double x2, double y2, double x3, double y3);
};
} // namespace osci