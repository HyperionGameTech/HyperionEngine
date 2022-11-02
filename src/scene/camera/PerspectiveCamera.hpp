#ifndef HYPERION_V2_PERSPECTIVE_CAMERA_H
#define HYPERION_V2_PERSPECTIVE_CAMERA_H

#include "Camera.hpp"

namespace hyperion::v2 {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov, int width, int height, float _near, float _far);
    virtual ~PerspectiveCamera() = default;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion::v2

#endif
