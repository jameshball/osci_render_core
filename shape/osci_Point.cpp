#include "osci_Point.h"

namespace osci {

float Point::EPSILON = 0.0001;

Point::Point() : x(0), y(0), z(0) {}

Point::Point(float val) : x(val), y(val), z(0) {}

Point::Point(float x, float y) : x(x), y(y), z(0) {}

Point::Point(float x, float y, float z) : x(x), y(y), z(z) {}

Point Point::nextVector(float drawingProgress){
    return Point(x, y, z);
}

void Point::rotate(float rotateX, float rotateY, float rotateZ) {
    // rotate around x-axis
    float cosValue = std::cos(rotateX);
    float sinValue = std::sin(rotateX);
    float y2 = cosValue * y - sinValue * z;
    float z2 = sinValue * y + cosValue * z;

    // rotate around y-axis
    cosValue = std::cos(rotateY);
    sinValue = std::sin(rotateY);
    float x2 = cosValue * x + sinValue * z2;
    z = -sinValue * x + cosValue * z2;

    // rotate around z-axis
    cosValue = cos(rotateZ);
    sinValue = sin(rotateZ);
    x = cosValue * x2 - sinValue * y2;
    y = sinValue * x2 + cosValue * y2;
}

void Point::normalize() {
    float mag = magnitude();
    x /= mag;
    y /= mag;
    z /= mag;
}

float Point::innerProduct(Point& other) {
    return x * other.x + y * other.y + z * other.z;
}

void Point::scale(float x, float y, float z) {
    this->x *= x;
    this->y *= y;
    this->z *= z;
}

void Point::translate(float x, float y, float z) {
    this->x += x;
    this->y += y;
    this->z += z;
}

float Point::length() {
    return 0.0;
}

float Point::magnitude() {
    return sqrt(x * x + y * y + z * z);
}

std::unique_ptr<Shape> Point::clone() {
    return std::make_unique<Point>(x, y, z);
}

std::string Point::type() {
    return std::string();
}

Point& Point::operator=(const Point& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
}

bool Point::operator==(const Point& other) {
    return std::abs(x - other.x) < EPSILON && std::abs(y - other.y) < EPSILON && std::abs(z - other.z) < EPSILON;
}

bool Point::operator!=(const Point& other) {
    return !(*this == other);
}

Point Point::operator+(const Point& other) {
    return Point(x + other.x, y + other.y, z + other.z);
}

Point Point::operator+(float scalar) {
    return Point(x + scalar, y + scalar, z + scalar);
}



Point Point::operator-(const Point& other) {
    return Point(x - other.x, y - other.y, z - other.z);
}

Point Point::operator-() {
    return Point(-x, -y, -z);
}

Point Point::operator-(float scalar) {
    return Point(x - scalar, y - scalar, z - scalar);
}

Point Point::operator*(const Point& other) {
    return Point(x * other.x, y * other.y, z * other.z);
}

Point Point::operator*(float scalar) {
    return Point(x * scalar, y * scalar, z * scalar);
}

Point Point::operator/(float scalar) {
    return Point(x / scalar, y / scalar, z / scalar);
}

// Compound assignment operators
Point& Point::operator+=(const Point& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Point& Point::operator+=(float scalar) {
    x += scalar;
    y += scalar;
    z += scalar;
    return *this;
}

Point& Point::operator-=(const Point& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

Point& Point::operator-=(float scalar) {
    x -= scalar;
    y -= scalar;
    z -= scalar;
    return *this;
}

Point& Point::operator*=(const Point& other) {
    x *= other.x;
    y *= other.y;
    z *= other.z;
    return *this;
}

Point& Point::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

Point& Point::operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
}

std::string Point::toString() {
    return std::string("(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
}

Point operator+(float scalar, const Point& point) {
    return Point(point.x + scalar, point.y + scalar, point.z + scalar);
}

Point operator-(float scalar, const Point& point) {
    return Point(point.x - scalar, point.y - scalar, point.z - scalar);
}

Point operator*(float scalar, const Point& point) {
    return Point(point.x * scalar, point.y * scalar, point.z * scalar);
}

Point operator/(float scalar, const Point& point) {
    return Point(scalar / point.x, scalar / point.y, scalar / point.z);
}

} // namespace osci
