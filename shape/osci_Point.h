#pragma once

#include "osci_Shape.h"
#include <cmath>
#include <string>

namespace osci {
class Point : public Shape {
public:
    Point(float x, float y, float z);
    Point(float x, float y);
    Point(float val);
    Point();

    Point nextVector(float drawingProgress) override;
    void scale(float x, float y, float z) override;
    void translate(float x, float y, float z) override;
    float length() override;
    float magnitude();
    std::unique_ptr<Shape> clone() override;
    std::string type() override;

    void rotate(float rotateX, float rotateY, float rotateZ);
    void normalize();
    float innerProduct(Point& other);

    std::string toString();

    Point& operator=(const Point& other);
    bool operator==(const Point& other);
    bool operator!=(const Point& other);
    Point operator+(const Point& other);
    Point operator+(float scalar);
    friend Point operator+(float scalar, const Point& point);
    Point operator-(const Point& other);
    Point operator-();
    Point operator-(float scalar);
    friend Point operator-(float scalar, const Point& point);
    Point operator*(const Point& other);
    Point operator*(float scalar);
    friend Point operator*(float scalar, const Point& point);
    Point operator/(float scalar);
    friend Point operator/(float scalar, const Point& point);

    // Compound assignment operators
    Point& operator+=(const Point& other);
    Point& operator+=(float scalar);
    Point& operator-=(const Point& other);
    Point& operator-=(float scalar);
    Point& operator*=(const Point& other);
    Point& operator*=(float scalar);
    Point& operator/=(float scalar);

    float x, y, z;
    
    static float EPSILON;
};
} // namespace osci
