#pragma once

#include <cmath>
#include <cstdlib>
#include <vector>
#include <memory>
#include <string>

namespace osci {
class Point;
class Shape {
public:
	virtual Point nextVector(float drawingProgress) = 0;
	virtual void scale(float x, float y, float z) = 0;
	virtual void translate(float x, float y, float z) = 0;
	virtual float length() = 0;
	virtual std::unique_ptr<Shape> clone() = 0;
	virtual std::string type() = 0;

	static float totalLength(std::vector<std::unique_ptr<Shape>>&);
	static void normalize(std::vector<std::unique_ptr<Shape>>&, float, float);
	static void normalize(std::vector<std::unique_ptr<Shape>>&);
	static float height(std::vector<std::unique_ptr<Shape>>&);
	static float width(std::vector<std::unique_ptr<Shape>>&);
	static Point maxVector(std::vector<std::unique_ptr<Shape>>&);
	static void removeOutOfBounds(std::vector<std::unique_ptr<Shape>>&);

	const float INVALID_LENGTH = -1.0;

	float len = INVALID_LENGTH;
};
} // namespace osci