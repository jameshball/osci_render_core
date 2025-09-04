#include "osci_Point.h"

namespace osci {

double Point::EPSILON = 0.0001;

Point::Point() : x(0), y(0), z(0), r(0), g(0), b(0) {}

Point::Point(double val) : x(val), y(val), z(0), r(0), g(0), b(0) {}

Point::Point(double x, double y) : x(x), y(y), z(0), r(0), g(0), b(0) {}

Point::Point(double x, double y, double z) : x(x), y(y), z(z), r(z), g(z), b(z) {}

Point::Point(double x, double y, double z, double r_, double g_, double b_) : x(x), y(y), z(z), r(r_), g(g_), b(b_) {}

Point Point::withColour(double r_, double g_, double b_) const {
    return Point(x, y, z, r_, g_, b_);
}

Point Point::nextVector(double drawingProgress){
    return Point(x, y, z);
}

void Point::rotate(double rotateX, double rotateY, double rotateZ) {
    // rotate around x-axis
    double cosValue = std::cos(rotateX);
    double sinValue = std::sin(rotateX);
    double y2 = cosValue * y - sinValue * z;
    double z2 = sinValue * y + cosValue * z;

    // rotate around y-axis
    cosValue = std::cos(rotateY);
    sinValue = std::sin(rotateY);
    double x2 = cosValue * x + sinValue * z2;
    z = -sinValue * x + cosValue * z2; // colour channels unaffected by rotation

    // rotate around z-axis
    cosValue = cos(rotateZ);
    sinValue = sin(rotateZ);
    x = cosValue * x2 - sinValue * y2;
    y = sinValue * x2 + cosValue * y2;
}

void Point::normalize() {
    double mag = magnitude();
    x /= mag;
    y /= mag;
    z /= mag; // leave colour magnitude unchanged
}

double Point::innerProduct(Point& other) {
    return x * other.x + y * other.y + z * other.z;
}

void Point::scale(double x, double y, double z) {
    this->x *= x;
    this->y *= y;
    this->z *= z; // colour not auto-scaled; treat colour separately upstream if needed
}

void Point::translate(double x, double y, double z) {
    this->x += x;
    this->y += y;
    this->z += z;
}

double Point::length() {
    return 0.0;
}

double Point::magnitude() {
    return sqrt(x * x + y * y + z * z);
}

std::unique_ptr<Shape> Point::clone() {
    return std::make_unique<Point>(x, y, z, r, g, b);
}

std::string Point::type() {
    return std::string();
}

Point& Point::operator=(const Point& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    r = other.r; g = other.g; b = other.b;
    return *this;
}

bool Point::operator==(const Point& other) {
    return std::abs(x - other.x) < EPSILON && std::abs(y - other.y) < EPSILON && std::abs(z - other.z) < EPSILON &&
           std::abs(r - other.r) < EPSILON && std::abs(g - other.g) < EPSILON && std::abs(b - other.b) < EPSILON;
}

bool Point::operator!=(const Point& other) {
    return !(*this == other);
}

Point Point::operator+(const Point& other) {
    return Point(x + other.x, y + other.y, z + other.z, r + other.r, g + other.g, b + other.b);
}

Point Point::operator+(double scalar) {
    return Point(x + scalar, y + scalar, z + scalar, r, g, b);
}



Point Point::operator-(const Point& other) {
    return Point(x - other.x, y - other.y, z - other.z, r - other.r, g - other.g, b - other.b);
}

Point Point::operator-() {
    return Point(-x, -y, -z, -r, -g, -b);
}

Point Point::operator-(double scalar) {
    return Point(x - scalar, y - scalar, z - scalar, r, g, b);
}

Point Point::operator*(const Point& other) {
    return Point(x * other.x, y * other.y, z * other.z, r * other.r, g * other.g, b * other.b);
}

Point Point::operator*(double scalar) {
    return Point(x * scalar, y * scalar, z * scalar, r * scalar, g * scalar, b * scalar);
}

Point Point::operator/(double scalar) {
    return Point(x / scalar, y / scalar, z / scalar, r / scalar, g / scalar, b / scalar);
}

// Compound assignment operators
Point& Point::operator+=(const Point& other) {
    x += other.x;
    y += other.y;
    z += other.z; r += other.r; g += other.g; b += other.b;
    return *this;
}

Point& Point::operator+=(double scalar) {
    x += scalar;
    y += scalar;
    z += scalar; // colour unchanged for scalar add
    return *this;
}

Point& Point::operator-=(const Point& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z; r -= other.r; g -= other.g; b -= other.b;
    return *this;
}

Point& Point::operator-=(double scalar) {
    x -= scalar;
    y -= scalar;
    z -= scalar; // colour unchanged for scalar subtract
    return *this;
}

Point& Point::operator*=(const Point& other) {
    x *= other.x;
    y *= other.y;
    z *= other.z; r *= other.r; g *= other.g; b *= other.b;
    return *this;
}

Point& Point::operator*=(double scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar; r *= scalar; g *= scalar; b *= scalar;
    return *this;
}

Point& Point::operator/=(double scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar; r /= scalar; g /= scalar; b /= scalar;
    return *this;
}

std::string Point::toString() {
    return std::string("(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ", rgb=" +
                       std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ")");
}

Point operator+(double scalar, const Point& point) {
    return Point(point.x + scalar, point.y + scalar, point.z + scalar, point.r, point.g, point.b);
}

Point operator-(double scalar, const Point& point) {
    return Point(point.x - scalar, point.y - scalar, point.z - scalar, point.r, point.g, point.b);
}

Point operator*(double scalar, const Point& point) {
    return Point(point.x * scalar, point.y * scalar, point.z * scalar, point.r * scalar, point.g * scalar, point.b * scalar);
}

Point operator/(double scalar, const Point& point) {
    return Point(scalar / point.x, scalar / point.y, scalar / point.z, scalar / point.r, scalar / point.g, scalar / point.b);
}

} // namespace osci
