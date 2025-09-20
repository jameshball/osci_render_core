#pragma once

#include "osci_Shape.h"
#include <cmath>
#include <string>

namespace osci {
class Point : public Shape {
public:
    // Constructors: legacy behaviour keeps z as brightness & mirrors into r=g=b when RGB not specified
    Point(float x, float y, float z, float r, float g, float b);
    Point(float x, float y, float z); // legacy: z replicated to RGB
    Point(float x, float y);           // legacy: z=0, RGB=0
    Point(float val);                   // all coords=val, brightness=0, RGB=0
    Point();

    // Helper to attach colour to an existing point (non-mutating)
    Point withColour(float r, float g, float b) const;

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

    float x, y, z; // spatial + legacy brightness/intensity
    float r, g, b; // colour channels (0..1 expected)
    
    static float EPSILON;
};
} // namespace osci
