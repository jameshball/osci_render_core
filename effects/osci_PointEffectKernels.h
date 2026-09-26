#pragma once

#include "../shape/osci_Point.h"
#include <algorithm>
#include <cmath>
#include <numbers>

// Stateless geometry shared by parameter-driven Render effects and authored
// Motion curves. Values retain the existing Render parameter units.
namespace osci::point_effects {
inline Point rotate(Point input, double x, double y, double z) {
    input.rotate(x * std::numbers::pi, y * std::numbers::pi, z * std::numbers::pi);
    return input;
}

inline Point scale(Point input, double x, double y, double z) {
    return (input * Point(x, y, z)).withColour(input.r, input.g, input.b);
}

inline Point translate(Point input, double x, double y, double z) {
    return (input + Point(x, y, z)).withColour(input.r, input.g, input.b);
}

inline Point skew(Point input, double x, double y, double z) {
    auto output = input;
    output.x += x * input.y;
    output.y += y * input.z;
    output.z += z * input.x;
    return output;
}

inline Point swirl(Point input, double amount) {
    const double length = 10 * amount * input.magnitude();
    const double x = input.x * std::cos(length) - input.y * std::sin(length);
    const double y = input.x * std::sin(length) + input.y * std::cos(length);
    return Point(x, y, input.z).withColour(input.r, input.g, input.b);
}

inline Point bulge(Point input, double amount) {
    const double radius = std::hypot(input.x, input.y);
    if (radius == 0) {
        return input;
    }
    const double scale = std::pow(radius, 1 - amount) / radius;
    return Point(scale * input.x, scale * input.y, input.z).withColour(input.r, input.g, input.b);
}

inline Point ripple(Point input, double depth, double phase, double amount) {
    const double distance = 100 * amount * (input.x * input.x + input.y * input.y);
    input.z += depth * std::sin(phase * std::numbers::pi + distance);
    return input;
}

inline Point vortex(Point input, double strength, double amount, double rotation) {
    const double effectScale = std::clamp(strength, 0.0, 1.0);
    const double exponent = std::max(1.0, std::floor(amount + 0.001));
    const double refTheta = rotation * std::numbers::pi * 2;
    Point output(0, 0, input.z);
    if (input.x != 0 || input.y != 0) {
        const double radiusSquared = input.x * input.x + input.y * input.y;
        const double theta = std::atan2(input.y, input.x) - refTheta;
        const double radius = std::pow(radiusSquared, 0.5 * exponent);
        const double angle = exponent * theta + refTheta;
        output.x = radius * std::cos(angle);
        output.y = radius * std::sin(angle);
    }
    return ((1 - effectScale) * input + effectScale * output).withColour(input.r, input.g, input.b);
}
}
