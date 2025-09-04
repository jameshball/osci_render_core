#pragma once

#include "osci_Shape.h"
#include <cmath>
#include <string>

namespace osci {
class Point : public Shape {
public:
    // Constructors: legacy behaviour keeps z as brightness & mirrors into r=g=b when RGB not specified
    Point(double x, double y, double z, double r, double g, double b);
    Point(double x, double y, double z); // legacy: z replicated to RGB
    Point(double x, double y);           // legacy: z=0, RGB=0
    Point(double val);                   // all coords=val, brightness=0, RGB=0
    Point();

    // Helper to attach colour to an existing point (non-mutating)
    Point withColour(double r, double g, double b) const;

    Point nextVector(double drawingProgress) override;
    void scale(double x, double y, double z) override;
    void translate(double x, double y, double z) override;
    double length() override;
    double magnitude();
    std::unique_ptr<Shape> clone() override;
    std::string type() override;

    void rotate(double rotateX, double rotateY, double rotateZ);
    void normalize();
    double innerProduct(Point& other);

    std::string toString();

    Point& operator=(const Point& other);
    bool operator==(const Point& other);
    bool operator!=(const Point& other);
    Point operator+(const Point& other);
    Point operator+(double scalar);
    friend Point operator+(double scalar, const Point& point);
    Point operator-(const Point& other);
    Point operator-();
    Point operator-(double scalar);
    friend Point operator-(double scalar, const Point& point);
    Point operator*(const Point& other);
    Point operator*(double scalar);
    friend Point operator*(double scalar, const Point& point);
    Point operator/(double scalar);
    friend Point operator/(double scalar, const Point& point);

    // Compound assignment operators
    Point& operator+=(const Point& other);
    Point& operator+=(double scalar);
    Point& operator-=(const Point& other);
    Point& operator-=(double scalar);
    Point& operator*=(const Point& other);
    Point& operator*=(double scalar);
    Point& operator/=(double scalar);

    double x, y, z; // spatial + legacy brightness/intensity
    double r, g, b; // colour channels (0..1 expected)
    
    static double EPSILON;
};
} // namespace osci
