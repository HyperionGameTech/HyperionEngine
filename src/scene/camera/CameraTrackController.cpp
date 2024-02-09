#include "CameraTrackController.hpp"

namespace hyperion::v2 {
CameraTrackController::CameraTrackController()
    : PerspectiveCameraController(),
      m_track_time(0.0)
{
}

CameraTrackController::CameraTrackController(RC<CameraTrack> camera_track)
    : PerspectiveCameraController(),
      m_camera_track(std::move(camera_track)),
      m_track_time(0.0)
{
}

void CameraTrackController::UpdateLogic(double dt)
{
    if (!m_camera_track) {
        return;
    }

    m_track_time += dt;

    const double current_track_time = std::fmod(m_track_time, m_camera_track->GetDuration());

    const CameraTrackPivot pivot = m_camera_track->GetPivotAt(current_track_time);

    const Vector3 view_vector = (pivot.transform.GetRotation() * -Vector3::UnitZ()).Normalized();

    DebugLog(LogType::Debug, "CameraTrackController::UpdateLogic() %f %f %f\n", view_vector.x, view_vector.y, view_vector.z);

    m_camera->SetNextTranslation(pivot.transform.GetTranslation());
    m_camera->SetDirection(view_vector);
}

void CameraTrackController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
}

} // namespace hyperion::v2
