/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_PERSPECTIVE_CAMERA_H
#define HYPERION_V2_PERSPECTIVE_CAMERA_H

#include <scene/camera/Camera.hpp>

namespace hyperion::v2 {
class HYP_API PerspectiveCameraController : public CameraController
{
public:
    PerspectiveCameraController();
    virtual ~PerspectiveCameraController() override = default;

    virtual void OnAdded(Camera *camera) override;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion::v2

#endif
