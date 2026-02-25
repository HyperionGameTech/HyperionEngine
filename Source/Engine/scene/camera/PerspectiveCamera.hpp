/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API PerspectiveCameraController : public CameraController
{
    HYP_OBJECT_BODY(PerspectiveCameraController);

public:
    PerspectiveCameraController();
    virtual ~PerspectiveCameraController() override = default;

    virtual void UpdateLogic(double delta) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
};

} // namespace Hyperion
