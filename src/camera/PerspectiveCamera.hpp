#ifndef PERSPECTIVE_CAMERA_H
#define PERSPECTIVE_CAMERA_H

#include "Camera.hpp"

namespace hyperion {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov, int width, int height, float _near, float _far);
    virtual ~PerspectiveCamera() = default;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion

#endif
