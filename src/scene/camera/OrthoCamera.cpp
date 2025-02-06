/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/OrthoCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {
OrthoCameraController::OrthoCameraController()
    : OrthoCameraController(
          -100.0f, 100.0f,
          -100.0f, 100.0f,
          -100.0f, 100.0f
      )
{
}

OrthoCameraController::OrthoCameraController(float left, float right, float bottom, float top, float _near, float _far)
    : CameraController(CameraProjectionMode::ORTHOGRAPHIC),
      m_left(left),
      m_right(right),
      m_bottom(bottom),
      m_top(top),
      m_near(_near),
      m_far(_far)
{
}

void OrthoCameraController::OnActivated()
{
    HYP_SCOPE;

    CameraController::OnActivated();

    m_camera->SetToOrthographicProjection(
        m_left, m_right,
        m_bottom, m_top,
        m_near, m_far
    );
}

void OrthoCameraController::OnDeactivated()
{
    HYP_SCOPE;
    
    CameraController::OnDeactivated();
}

void OrthoCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;
}

void OrthoCameraController::UpdateViewMatrix()
{
    HYP_SCOPE;
    
    m_camera->m_view_mat = Matrix4::LookAt(
        m_camera->m_translation,
        m_camera->GetTarget(),
        m_camera->m_up
    );
}

void OrthoCameraController::UpdateProjectionMatrix()
{
    HYP_SCOPE;
    
    m_camera->SetToOrthographicProjection(
        m_camera->m_left,   m_camera->m_right,
        m_camera->m_bottom, m_camera->m_top,
        m_camera->m_near,   m_camera->m_far
    );
}
} // namespace hyperion
