#pragma once

#include "osci_Shape.h"
#include "osci_Point.h"

namespace osci {
class CircleArc : public Shape {
public:
	CircleArc(double x, double y, double radiusX, double radiusY, double startAngle, double endAngle);

	Point nextVector(double drawingProgress) override;
	void scale(double x, double y, double z) override;
	void translate(double x, double y, double z) override;
	double length() override;
	std::unique_ptr<Shape> clone() override;
	std::string type() override;

	double x, y, radiusX, radiusY, startAngle, endAngle;
	
};
} // namespace osci