#include "PerspectiveCamera.hpp"

namespace hyperion::v2 {
PerspectiveCamera::PerspectiveCamera(float fov, int width, int height, float _near, float _far)
    : Camera(CameraType::PERSPECTIVE, width, height, _near, _far)
{
    m_fov = fov;
}

void PerspectiveCamera::UpdateLogic(double dt)
{
}

void PerspectiveCamera::UpdateViewMatrix()
{
    m_view_mat = Matrix4::LookAt(
        m_translation,
        GetTarget(),
        m_up
    );
}

void PerspectiveCamera::UpdateProjectionMatrix()
{
    m_proj_mat = Matrix4::Perspective(
        m_fov,
        m_width, m_height,
        m_near,  m_far
    );
}
} // namespace hyperion::v2
