#pragma once

#include <numbers>

namespace osci {
    
class Util {
public:
    
    // from https://stackoverflow.com/questions/11980292/how-to-wrap-around-a-range
    static inline double wrapAngle(double angle) {
        constexpr double twoPi = 2.0 * std::numbers::pi;
        constexpr double pi = std::numbers::pi;
        return std::fmod(std::fmod(angle + pi, twoPi) + twoPi, twoPi) - pi;
    }
};

} // namespace osci
