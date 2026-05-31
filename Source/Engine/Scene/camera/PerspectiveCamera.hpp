/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API PerspectiveCameraController : public CameraController
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
