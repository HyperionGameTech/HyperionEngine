/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/CameraTrackController.hpp>

namespace hyperion {
CameraTrackController::CameraTrackController()
    : PerspectiveCameraController(),
      m_trackTime(0.0)
{
}

CameraTrackController::CameraTrackController(RC<CameraTrack> cameraTrack)
    : PerspectiveCameraController(),
      m_cameraTrack(std::move(cameraTrack)),
      m_trackTime(0.0)
{
}

void CameraTrackController::UpdateLogic(double dt)
{
    if (!m_cameraTrack)
    {
        return;
    }

    m_trackTime += dt;

    const double currentTrackTime = std::fmod(m_trackTime, m_cameraTrack->GetDuration());

    const CameraTrackPivot pivot = m_cameraTrack->GetPivotAt(currentTrackTime);

    const Vector3 viewVector = (pivot.transform.GetRotation() * -Vector3::UnitZ()).Normalized();

    m_camera->SetNextTranslation(pivot.transform.GetTranslation());
    m_camera->SetDirection(viewVector);
}

void CameraTrackController::RespondToCommand(const CameraCommand& command, float dt)
{
}

} // namespace hyperion
