/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <OrthoCamera.generated.inl>

namespace Hyperion {
OrthoCameraController::OrthoCameraController()
    : OrthoCameraController(
          -100.0f, 100.0f,
          -100.0f, 100.0f)
{
}

OrthoCameraController::OrthoCameraController(float left, float right, float bottom, float top)
    : CameraController(CameraProjectionMode::ORTHOGRAPHIC)
{
    m_rect.left = left;
    m_rect.right = right;
    m_rect.bottom = bottom;
    m_rect.top = top;
}

void OrthoCameraController::OnActivated()
{
    CameraController::OnActivated();

    m_camera->SetToOrthographicProjection(
        m_rect.left, m_rect.right,
        m_rect.bottom, m_rect.top,
        m_camera->GetNearClip(), m_camera->GetFarClip());
}

void OrthoCameraController::OnDeactivated()
{
    CameraController::OnDeactivated();
}

void OrthoCameraController::UpdateLogic(double delta)
{
}

void OrthoCameraController::UpdateViewMatrix()
{
    m_camera->m_viewMat = Mat4f::LookAt(
        m_camera->GetWorldTranslation(),
        m_camera->GetTarget(),
        m_camera->m_up);
}

void OrthoCameraController::UpdateProjectionMatrix()
{
    m_camera->SetToOrthographicProjection(
        m_rect.left, m_rect.right,
        m_rect.bottom, m_rect.top,
        m_camera->GetNearClip(), m_camera->GetFarClip());
}
} // namespace Hyperion
