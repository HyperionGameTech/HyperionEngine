/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Camera/PerspectiveCamera.hpp>

#include <PerspectiveCamera.generated.inl>

namespace Hyperion {

PerspectiveCameraController::PerspectiveCameraController()
    : CameraController(CameraProjectionMode::PERSPECTIVE)
{
}

void PerspectiveCameraController::OnActivated()
{
    CameraController::OnActivated();
}

void PerspectiveCameraController::OnDeactivated()
{
    CameraController::OnDeactivated();
}

void PerspectiveCameraController::UpdateLogic(double delta)
{
}

void PerspectiveCameraController::UpdateViewMatrix()
{
    m_camera->m_viewMat = Mat4f::LookAt(
        m_camera->GetWorldTranslation(),
        m_camera->GetTarget(),
        m_camera->m_up);
}

void PerspectiveCameraController::UpdateProjectionMatrix()
{
    m_camera->SetToPerspectiveProjection(
        m_camera->m_fov,
        m_camera->m_near,
        m_camera->m_far);
}

} // namespace Hyperion
