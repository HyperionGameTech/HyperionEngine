/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/camera/CameraTrackController.hpp>

#include <CameraTrackController.generated.inl>

namespace Hyperion {
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

void CameraTrackController::UpdateLogic(double delta)
{
    if (!m_cameraTrack)
    {
        return;
    }

    m_trackTime += delta;

    const double currentTrackTime = std::fmod(m_trackTime, m_cameraTrack->GetDuration());

    const CameraTrackPivot pivot = m_cameraTrack->GetPivotAt(currentTrackTime);

    const Vec3f viewVector = pivot.transform.GetRotation().RotateVector(-Vec3f::UnitZ()).Normalized();

    m_camera->SetNextTranslation(pivot.transform.GetTranslation());
    m_camera->SetDirection(viewVector);
}

} // namespace Hyperion
