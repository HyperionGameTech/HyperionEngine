/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_TRACK_CONTROLLER_HPP
#define HYPERION_CAMERA_TRACK_CONTROLLER_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/CameraTrack.hpp>

namespace hyperion {

HYP_CLASS()

class CameraTrackController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(CameraTrackController);

public:
    CameraTrackController();
    CameraTrackController(RC<CameraTrack> camera_track);
    virtual ~CameraTrackController() = default;

    const RC<CameraTrack>& GetCameraTrack() const
    {
        return m_camera_track;
    }

    void SetCameraTrack(RC<CameraTrack> camera_track)
    {
        m_camera_track = std::move(camera_track);
    }

    virtual void UpdateLogic(double dt) override;

protected:
    RC<CameraTrack> m_camera_track;
    double m_track_time;

private:
    virtual void RespondToCommand(const CameraCommand& command, GameCounter::TickUnit dt) override;
};

} // namespace hyperion

#endif
