#ifndef HYPERION_V2_CAMERA_TRACK_CONTROLLER_HPP
#define HYPERION_V2_CAMERA_TRACK_CONTROLLER_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/CameraTrack.hpp>

namespace hyperion::v2 {
class CameraTrackController : public PerspectiveCameraController
{
public:
    CameraTrackController();
    CameraTrackController(RC<CameraTrack> camera_track);
    virtual ~CameraTrackController() = default;

    const RC<CameraTrack> &GetCameraTrack() const
        { return m_camera_track; }

    void SetCameraTrack(RC<CameraTrack> camera_track)
        { m_camera_track = std::move(camera_track); }

    virtual void UpdateLogic(double dt) override;

protected:
    RC<CameraTrack> m_camera_track;
    double m_track_time;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;
};
} // namespace hyperion::v2

#endif
