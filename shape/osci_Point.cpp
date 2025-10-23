#include "osci_Point.h"
#include <JuceHeader.h>

namespace osci {

float Point::EPSILON = 0.0001;

Point::Point() : x(0), y(0), z(0), r(0), g(0), b(0) {}

Point::Point(float val) : x(val), y(val), z(0), r(0), g(0), b(0) {}

Point::Point(float x, float y) : x(x), y(y), z(0), r(0), g(0), b(0) {}

Point::Point(float x, float y, float z) : x(x), y(y), z(z), r(z), g(z), b(z) {}

Point::Point(float x, float y, float z, float r_, float g_, float b_) : x(x), y(y), z(z), r(r_), g(g_), b(b_) {}

Point Point::withColour(float r_, float g_, float b_) const {
    return Point(x, y, z, r_, g_, b_);
}

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
    z = -sinValue * x + cosValue * z2; // colour channels unaffected by rotation

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
    z /= mag; // leave colour magnitude unchanged
}

float Point::innerProduct(Point& other) {
    return x * other.x + y * other.y + z * other.z;
}

void Point::scale(float x, float y, float z) {
    this->x *= x;
    this->y *= y;
    this->z *= z; // colour not auto-scaled; treat colour separately upstream if needed
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

Point Point::operator+(float scalar) {
    return Point(x + scalar, y + scalar, z + scalar, r, g, b);
}



Point Point::operator-(const Point& other) {
    return Point(x - other.x, y - other.y, z - other.z, r - other.r, g - other.g, b - other.b);
}

Point Point::operator-() {
    return Point(-x, -y, -z, r, g, b);
}

Point Point::operator-(float scalar) {
    return Point(x - scalar, y - scalar, z - scalar, r, g, b);
}

Point Point::operator*(const Point& other) {
    return Point(x * other.x, y * other.y, z * other.z, r * other.r, g * other.g, b * other.b);
}

Point Point::operator*(float scalar) {
    return Point(x * scalar, y * scalar, z * scalar, r, g, b);
}

Point Point::operator/(float scalar) {
    return Point(x / scalar, y / scalar, z / scalar, r, g, b);
}

// Compound assignment operators
Point& Point::operator+=(const Point& other) {
    x += other.x;
    y += other.y;
    z += other.z; r += other.r; g += other.g; b += other.b;
    return *this;
}

Point& Point::operator+=(float scalar) {
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

Point& Point::operator-=(float scalar) {
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

Point& Point::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar; r *= scalar; g *= scalar; b *= scalar;
    return *this;
}

Point& Point::operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar; r /= scalar; g /= scalar; b /= scalar;
    return *this;
}

std::string Point::toString() {
    return std::string("(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ", rgb=" +
                       std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ")");
}

float& Point::operator[](int index) {
    switch (index) {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        case 3: return r;
        case 4: return g;
        case 5: return b;
        default: 
            jassertfalse; // Invalid index: must be 0-5
            return x;
    }
}

float Point::operator[](int index) const {
    switch (index) {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        case 3: return r;
        case 4: return g;
        case 5: return b;
        default: 
            jassertfalse; // Invalid index: must be 0-5
            return x;
    }
}

Point Point::fromAudioBuffer(const juce::AudioBuffer<float>& buffer, int sampleIndex) {
    Point point;
    const int numChannels = buffer.getNumChannels();
    
    // Assert only if more than 6 channels
    jassert(numChannels <= 6);
    
    // Fill all available channels up to 6 (x, y, z, r, g, b)
    const int channelsToFill = juce::jmin(numChannels, 6);
    for (int ch = 0; ch < channelsToFill; ++ch) {
        point[ch] = buffer.getReadPointer(ch)[sampleIndex];
    }
    
    return point;
}

Point operator+(float scalar, const Point& point) {
    return Point(point.x + scalar, point.y + scalar, point.z + scalar, point.r, point.g, point.b);
}

Point operator-(float scalar, const Point& point) {
    return Point(point.x - scalar, point.y - scalar, point.z - scalar, point.r, point.g, point.b);
}

Point operator*(float scalar, const Point& point) {
    return Point(point.x * scalar, point.y * scalar, point.z * scalar, point.r, point.g, point.b);
}

Point operator/(float scalar, const Point& point) {
    return Point(scalar / point.x, scalar / point.y, scalar / point.z, point.r, point.g, point.b);
}

} // namespace osci
