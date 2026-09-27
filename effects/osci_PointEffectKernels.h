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

// Hue is degrees; saturation and brightness are multiplicative. RGB zero
// remains beam blanking, and unresolved inherited colour remains unresolved.
inline Point colour(Point input, double hue, double saturation, double brightness) {
    if (input.r < 0) { return input; }
    if (!std::isfinite(input.r) || !std::isfinite(input.g) || !std::isfinite(input.b)
        || !std::isfinite(hue) || !std::isfinite(saturation) || !std::isfinite(brightness)) {
        return input.withColour(0, 0, 0);
    }
    if (hue == 0 && saturation == 1 && brightness == 1) { return input; }
    const double r = std::clamp(input.r, 0.0f, 1.0f);
    const double g = std::clamp(input.g, 0.0f, 1.0f);
    const double b = std::clamp(input.b, 0.0f, 1.0f);
    const auto maximum = std::max({r, g, b});
    const auto chroma = maximum - std::min({r, g, b});
    if (maximum == 0) { return input.withColour(0, 0, 0); }
    double angle = 0;
    if (chroma > 0) {
        if (maximum == r) {
            angle = (g - b) / chroma;
        } else if (maximum == g) {
            angle = 2 + (b - r) / chroma;
        } else {
            angle = 4 + (r - g) / chroma;
        }
    }
    angle = std::fmod(angle + hue / 60, 6.0);
    if (angle < 0) { angle += 6; }
    const auto value = std::clamp(maximum * brightness, 0.0, 1.0);
    const auto adjustedChroma = value * std::clamp(chroma / maximum * saturation, 0.0, 1.0);
    const auto secondary = adjustedChroma * (1 - std::abs(std::fmod(angle, 2.0) - 1));
    const auto floor = value - adjustedChroma;
    double red = 0, green = 0, blue = 0;
    if (angle < 1) {
        red = adjustedChroma; green = secondary;
    } else if (angle < 2) {
        red = secondary; green = adjustedChroma;
    } else if (angle < 3) {
        green = adjustedChroma; blue = secondary;
    } else if (angle < 4) {
        green = secondary; blue = adjustedChroma;
    } else if (angle < 5) {
        red = secondary; blue = adjustedChroma;
    } else {
        red = adjustedChroma; blue = secondary;
    }
    return input.withColour(red + floor, green + floor, blue + floor);
}

}
