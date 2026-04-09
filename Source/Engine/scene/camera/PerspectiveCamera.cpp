/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/camera/PerspectiveCamera.hpp>

#include <PerspectiveCamera.generated.inl>

namespace Hyperion {

PerspectiveCameraController::PerspectiveCameraController()
    : CameraController(CameraProjectionMode::PERSPECTIVE)
{
}

void PerspectiveCameraController::OnActivated()
{
    HYP_SCOPE;

    CameraController::OnActivated();
}

void PerspectiveCameraController::OnDeactivated()
{
    HYP_SCOPE;

    CameraController::OnDeactivated();
}

void PerspectiveCameraController::UpdateLogic(double delta)
{
    HYP_SCOPE;
}

void PerspectiveCameraController::UpdateViewMatrix()
{
    HYP_SCOPE;

    m_camera->m_viewMat = Mat4f::LookAt(
        m_camera->m_translation,
        m_camera->GetTarget(),
        m_camera->m_up);
}

void PerspectiveCameraController::UpdateProjectionMatrix()
{
    HYP_SCOPE;

    m_camera->SetToPerspectiveProjection(
        m_camera->m_fov,
        m_camera->m_near,
        m_camera->m_far);
}

} // namespace Hyperion
