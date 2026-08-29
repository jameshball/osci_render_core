#include "osci_PerspectiveProjector.h"

namespace osci {

void PerspectiveProjector::setFieldOfViewRadians(float newFieldOfViewRadians) {
    tangentHalfFov = std::tan(newFieldOfViewRadians * 0.5f);
    focalLength = 1.0f / tangentHalfFov;
}

void PerspectiveProjector::setCameraPosition(Vec3 newCameraPosition) {
    cameraPosition = newCameraPosition;
}

Vec3 PerspectiveProjector::toCameraSpace(Vec3 worldPoint) const {
    return Vec3(worldPoint.x - cameraPosition.x, worldPoint.y - cameraPosition.y, worldPoint.z - cameraPosition.z);
}

Vec3 PerspectiveProjector::project(Vec3 worldPoint) const {
    auto cameraPoint = toCameraSpace(worldPoint);
    cameraPoint.z = std::clamp(cameraPoint.z, nearPlane, farPlane);
    return Vec3(cameraPoint.x * focalLength / cameraPoint.z, cameraPoint.y * focalLength / cameraPoint.z, 0.0f);
}

} // namespace osci
