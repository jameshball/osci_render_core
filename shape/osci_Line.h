#pragma once

#include "osci_Shape.h"
#include "osci_Point.h"

namespace osci {
class Line : public Shape {
public:
	Line(float x1, float y1, float x2, float y2);
	Line(float x1, float y1, float z1, float x2, float y2, float z2);
	Line(Point p1, Point p2);

	Point nextVector(float drawingProgress) override;
	void scale(float x, float y, float z) override;
	void translate(float x, float y, float z) override;
	static float length(float x1, float y1, float z1, float x2, float y2, float z2);
	float length() override;
	std::unique_ptr<Shape> clone() override;
	std::string type() override;
	Line& operator=(const Line& other);

	float x1, y1, z1, x2, y2, z2;
	
};
} // namespace osci