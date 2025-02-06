/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/PerspectiveCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

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

void PerspectiveCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;
}

void PerspectiveCameraController::UpdateViewMatrix()
{
    HYP_SCOPE;
    
    m_camera->m_view_mat = Matrix4::LookAt(
        m_camera->m_translation,
        m_camera->GetTarget(),
        m_camera->m_up
    );
}

void PerspectiveCameraController::UpdateProjectionMatrix()
{
    HYP_SCOPE;
    
    m_camera->SetToPerspectiveProjection(
        m_camera->m_fov,
        m_camera->m_near,
        m_camera->m_far
    );
}

} // namespace hyperion
