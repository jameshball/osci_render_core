#pragma once

#include "osci_Shape.h"
#include "osci_Point.h"

namespace osci {
class CubicBezierCurve : public Shape {
public:
	CubicBezierCurve(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);

	Point nextVector(float drawingProgress) override;
	void scale(float x, float y, float z) override;
	void translate(float x, float y, float z) override;
	float length() override;
	std::unique_ptr<Shape> clone() override;
	std::string type() override;

	float x1, y1, x2, y2, x3, y3, x4, y4;
	
};
} // namespace osci