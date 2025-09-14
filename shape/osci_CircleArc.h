#pragma once

#include "osci_Shape.h"
#include "osci_Point.h"

namespace osci {
class CircleArc : public Shape {
public:
	CircleArc(float x, float y, float radiusX, float radiusY, float startAngle, float endAngle);

	Point nextVector(float drawingProgress) override;
	void scale(float x, float y, float z) override;
	void translate(float x, float y, float z) override;
	float length() override;
	std::unique_ptr<Shape> clone() override;
	std::string type() override;

	float x, y, radiusX, radiusY, startAngle, endAngle;
	
};
} // namespace osci