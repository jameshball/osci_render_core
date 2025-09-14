#include "osci_QuadraticBezierCurve.h"

namespace osci {
QuadraticBezierCurve::QuadraticBezierCurve(float x1, float y1, float x2, float y2, float x3, float y3) : CubicBezierCurve(x1, y1, x1 + (x2 - x1) * (2.0 / 3.0), y1 + (y2 - y1) * (2.0 / 3.0), x3 + (x2 - x3) * (2.0 / 3.0), y3 + (y2 - y3) * (2.0 / 3.0), x3, y3) {}
} // namespace osci