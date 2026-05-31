/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/camera/Camera.hpp>

namespace Hyperion {

HYP_CLASS()
class OrthoCameraController : public CameraController
{
    HYP_OBJECT_BODY(OrthoCameraController);

public:
    OrthoCameraController();
    OrthoCameraController(float left, float right, float bottom, float top);
    virtual ~OrthoCameraController() override = default;

    virtual void UpdateLogic(double delta) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

    CameraOrthoRect m_rect;
};

} // namespace Hyperion
