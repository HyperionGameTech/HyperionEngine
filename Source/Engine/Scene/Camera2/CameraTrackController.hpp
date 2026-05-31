/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/RefCountedPtr.hpp>

#include <Scene/camera/PerspectiveCamera.hpp>
#include <Scene/camera/CameraTrack.hpp>

namespace Hyperion {

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

} // namespace Hyperion
