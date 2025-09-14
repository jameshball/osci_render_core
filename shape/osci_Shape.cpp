#include "osci_Shape.h"
#include "osci_Line.h"
#include "osci_Point.h"

namespace osci {

float Shape::totalLength(std::vector<std::unique_ptr<Shape>>& shapes) {
    float length = 0.0;
	for (auto& shape : shapes) {
		length += shape->length();
	}
	return length;
}

void Shape::normalize(std::vector<std::unique_ptr<Shape>>& shapes, float width, float height) {
    float maxDim = std::max(width, height);
    
	for (auto& shape : shapes) {
        shape->scale(2.0 / maxDim, -2.0 / maxDim, 2.0 / maxDim);
		shape->translate(-1.0, 1.0, 0.0);
	}

	removeOutOfBounds(shapes);
}

void Shape::normalize(std::vector<std::unique_ptr<Shape>>& shapes) {
    float oldHeight = height(shapes);
	float oldWidth = width(shapes);
	float maxDim = std::max(oldHeight, oldWidth);

	for (auto& shape : shapes) {
		shape->scale(2.0 / maxDim, -2.0 / maxDim, 2.0 / maxDim);
	}

	Point max = maxVector(shapes);
	float newHeight = height(shapes);

	for (auto& shape : shapes) {
		shape->translate(-1.0, -max.y + newHeight / 2.0, 0.0);
	}
}

float Shape::height(std::vector<std::unique_ptr<Shape>>& shapes) {
	float maxY = std::numeric_limits<float>::min();
	float minY = std::numeric_limits<float>::max();

    Point vectors[4];

    for (auto& shape : shapes) {
        for (int i = 0; i < 4; i++) {
            vectors[i] = shape->nextVector(i * 1.0 / 4.0);
        }

        for (auto& vector : vectors) {
            maxY = std::max(maxY, vector.y);
            minY = std::min(minY, vector.y);
        }
    }

    return std::abs(maxY - minY);
}

float Shape::width(std::vector<std::unique_ptr<Shape>>& shapes) {
    float maxX = std::numeric_limits<float>::min();
    float minX = std::numeric_limits<float>::max();

    Point vectors[4];

    for (auto& shape : shapes) {
        for (int i = 0; i < 4; i++) {
            vectors[i] = shape->nextVector(i * 1.0 / 4.0);
        }

        for (auto& vector : vectors) {
            maxX = std::max(maxX, vector.x);
            minX = std::min(minX, vector.x);
        }
    }

    return std::abs(maxX - minX);
}

Point Shape::maxVector(std::vector<std::unique_ptr<Shape>>& shapes) {
    float maxX = std::numeric_limits<float>::min();
    float maxY = std::numeric_limits<float>::min();

    for (auto& shape : shapes) {
        Point startVector = shape->nextVector(0);
        Point endVector = shape->nextVector(1);

        float x = std::max(startVector.x, endVector.x);
        float y = std::max(startVector.y, endVector.y);

        maxX = std::max(x, maxX);
        maxY = std::max(y, maxY);
    }

	return Point(maxX, maxY);
}

void Shape::removeOutOfBounds(std::vector<std::unique_ptr<Shape>>& shapes) {
    std::vector<int> toRemove;

    for (int i = 0; i < shapes.size(); i++) {
		Point start = shapes[i]->nextVector(0);
		Point end = shapes[i]->nextVector(1);
        bool keep = false;

        if ((start.x < 1 && start.x > -1) || (start.y < 1 && start.y > -1)) {
            if ((end.x < 1 && end.x > -1) || (end.y < 1 && end.y > -1)) {
				if (shapes[i]->type() == "Line") {
                    Point newStart(std::min(std::max(start.x, -1.0f), 1.0f), std::min(std::max(start.y, -1.0f), 1.0f));
                    Point newEnd(std::min(std::max(end.x, -1.0f), 1.0f), std::min(std::max(end.y, -1.0f), 1.0f));
					shapes[i] = std::make_unique<Line>(newStart.x, newStart.y, newEnd.x, newEnd.y);
                }
                keep = true;
            }
        }

        if (!keep) {
            toRemove.push_back(i);
        }
    }

    for (int i = toRemove.size() - 1; i >= 0; i--) {
        shapes.erase(shapes.begin() + toRemove[i]);
    }
}

} // namespace osci