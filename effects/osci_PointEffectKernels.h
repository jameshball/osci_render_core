#pragma once

#include "../shape/osci_Point.h"
#include "../geometry/osci_PerspectiveProjector.h"
#include "../osci_Util.h"
#include <algorithm>
#include <cmath>
#include <numbers>

// Stateless point geometry shared by the effects and any host that applies
// them with its own parameter values, in the effects' parameter units.
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
    // In float, as the effect always has, so its output is unchanged.
    const double length = 10 * static_cast<float>(amount) * input.magnitude();
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
    const double distance = 100 * static_cast<float>(amount) * (input.x * input.x + input.y * input.y);
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

// Quantises to a grid that coarsens as `crush` (0..1) rises.
inline Point bitCrush(Point input, double crush) {
    const double power = std::pow(2.0f, 1.0 - crush * 0.78) - 1.0;
    const double quant = 0.5 * powf(2.0f, power * 12);
    const double dequant = 1.0f / quant;
    return Point(dequant * std::round(input.x * quant), dequant * std::round(input.y * quant), dequant * std::round(input.z * quant)).withColour(input.r, input.g, input.b);
}

// Turns each point about Y by an angle that grows with its height.
inline Point twist(Point input, double amount) {
    input.rotate(0.0, amount * 4 * std::numbers::pi * input.y, 0.0);
    return input;
}

// Snaps points onto stripes of a polygon (`sides`), turned by `rotation`
// (turns) and offset by `phase` (stripes). Theta is measured from +Y.
inline Point polygon(Point input, double sides, double stripeSize, double rotation, double phase) {
    constexpr double pi = std::numbers::pi, twoPi = 2 * std::numbers::pi;
    const double count = std::max(2.0, sides);
    const double stripe = std::pow(0.63 * std::max<double>(1e-4f, stripeSize), 1.5);
    Point output(0);
    if (input.x != 0 || input.y != 0) {
        const double r = std::hypot(input.x, input.y);
        const double theta = Util::wrapAngle(std::atan2(-input.x, input.y) - rotation * twoPi + pi) - pi;
        const double centre = std::round(theta * count / twoPi) / count * twoPi;
        const double distance = r * std::cos(theta - centre);
        const double snapped = std::max(0.0, (std::round(distance / stripe - phase) + phase) * stripe);
        output.x = snapped / distance * input.x;
        output.y = snapped / distance * input.y;
    }
    // Depth snaps to the same stripes.
    const double depth = std::abs(input.z);
    if (depth > 0.0001) {
        output.z = (input.z > 0 ? 1 : -1) * std::max(0.0, (std::round(depth / stripe - phase) + phase) * stripe);
    }
    return output.withColour(input.r, input.g, input.b);
}

// Snaps points to the cells of a log-polar spiral grid: `density` cells per
// turn, `spiralTwist` of them per ring, zoomed and turned (both in turns).
inline Point spiralCrush(Point input, double density, double spiralTwist, double zoomTurns, double rotationTurns) {
    constexpr double twoPi = 2 * std::numbers::pi;
    const double domainX = std::max(2.0, std::floor(density + 0.001));
    const double domainY = std::round(domainX * spiralTwist);
    const double zoom = zoomTurns * twoPi, rotation = rotationTurns * twoPi;
    const double domainTheta = std::atan2(domainY, domainX);
    const double scale = std::hypot(domainX, domainY) / twoPi;
    Point output(0);
    if (input.x != 0 || input.y != 0) {
        // One revolution traverses one domain's hypotenuse; theta is from -Y.
        const double radius = std::hypot(input.x, input.y);
        Point cell(std::atan2(input.x, -input.y) - rotation, std::log(radius) - zoom);
        cell.rotate(0, 0, domainTheta);
        cell = cell * scale;
        cell.x = std::round(cell.x);
        cell.y = std::round(cell.y);
        cell = cell / scale;
        cell.rotate(0, 0, -domainTheta);
        const double snapped = std::exp(cell.y + zoom), theta = cell.x + rotation;
        output.x = snapped * std::sin(theta);
        output.y = snapped * -std::cos(theta);
    }
    // Depth snaps in log space with the radial spacing and offset.
    if (input.z != 0) {
        const double logZ = std::round((std::log(std::abs(input.z)) - zoom) * scale) / scale + zoom;
        output.z = (input.z > 0 ? 1.0 : -1.0) * std::exp(logZ);
    }
    return output.withColour(input.r, input.g, input.b);
}

// A pinhole view whose cone just touches the unit sphere, flattened to z 0.
inline Point perspective(Point input, double fieldOfViewDegrees) {
    // Far-plane clipping starts at about 1.2 degrees.
    const float fov = std::clamp(static_cast<float>(fieldOfViewDegrees), 1.5f, 179.0f) * (std::numbers::pi_v<float> / 180.0f);
    const PerspectiveProjector projector(fov, Vec3(0, 0, -1.0f / std::sin(0.5f * fov)));
    const auto projected = projector.project(Vec3(input.x, input.y, input.z));
    return Point(projected.x, projected.y, 0).withColour(input.r, input.g, input.b);
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
