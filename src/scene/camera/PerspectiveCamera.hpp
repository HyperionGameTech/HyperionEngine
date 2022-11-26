#ifndef HYPERION_V2_PERSPECTIVE_CAMERA_H
#define HYPERION_V2_PERSPECTIVE_CAMERA_H

#include "Camera.hpp"

namespace hyperion::v2 {
class PerspectiveCameraController : public CameraController
{
public:
    PerspectiveCameraController();
    virtual ~PerspectiveCameraController() = default;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion::v2

#endif
