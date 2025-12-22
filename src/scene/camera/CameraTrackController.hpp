/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
    CameraTrackController(RC<CameraTrack> cameraTrack);
    virtual ~CameraTrackController() = default;

    const RC<CameraTrack>& GetCameraTrack() const
    {
        return m_cameraTrack;
    }

    void SetCameraTrack(RC<CameraTrack> cameraTrack)
    {
        m_cameraTrack = std::move(cameraTrack);
    }

    virtual void UpdateLogic(double delta) override;

protected:
    RC<CameraTrack> m_cameraTrack;
    double m_trackTime;
};

} // namespace hyperion
