#include "OrthoCamera.hpp"

namespace hyperion::v2 {
OrthoCameraController::OrthoCameraController()
    : CameraController(CameraType::ORTHOGRAPHIC)
{
}

void OrthoCameraController::UpdateLogic(double dt)
{
}

void OrthoCameraController::UpdateViewMatrix()
{
    m_camera->m_view_mat = Matrix4::LookAt(
        m_camera->m_translation,
        m_camera->GetTarget(),
        m_camera->m_up
    );
}

void OrthoCameraController::UpdateProjectionMatrix()
{
    m_camera->m_proj_mat = Matrix4::Orthographic(
        m_camera->m_left,   m_camera->m_right,
        m_camera->m_bottom, m_camera->m_top,
        m_camera->m_near,   m_camera->m_far
    );
}
} // namespace hyperion::v2
