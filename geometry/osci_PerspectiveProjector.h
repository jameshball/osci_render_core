#pragma once

#include <algorithm>
#include <cmath>

namespace osci {

struct Vec3 {
    Vec3() = default;
    Vec3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class PerspectiveProjector {
public:
    void setFieldOfViewRadians(float newFieldOfViewRadians);
    void setCameraPosition(Vec3 newCameraPosition);
    Vec3 project(Vec3 worldPoint) const;

private:
    static constexpr float nearPlane = 0.001f;
    static constexpr float farPlane = 100.0f;

    Vec3 toCameraSpace(Vec3 worldPoint) const;

    Vec3 cameraPosition;
    float tangentHalfFov = std::tan(0.5f);
    float focalLength = 1.0f / tangentHalfFov;
};

} // namespace osci
