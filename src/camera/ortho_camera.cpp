#include "ortho_camera.h"

namespace hyperion {
OrthoCamera::OrthoCamera(
    int width, int height,
    float left, float right,
    float bottom, float top,
    float _near, float _far
) : Camera(CameraType::ORTHOGRAPHIC, width, height, _near, _far),
    m_left(left),
    m_right(right),
    m_bottom(bottom),
    m_top(top)
{
}

void OrthoCamera::UpdateLogic(double dt)
{
}

void OrthoCamera::UpdateViewMatrix()
{
    m_view_mat = Matrix4::LookAt(
        m_translation,
        GetTarget(),
        m_up
    );
}

void OrthoCamera::UpdateProjectionMatrix()
{
    m_proj_mat = Matrix4::Orthographic(
        m_left,   m_right,
        m_bottom, m_top,
        m_near,   m_far
    );
}
}
