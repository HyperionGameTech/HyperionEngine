/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/SharedPtr.hpp>

#include <Scene/Camera/PerspectiveCamera.hpp>
#include <Scene/Camera/CameraTrack.hpp>

namespace Hyperion {

HYP_CLASS()
class CameraTrackController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(CameraTrackController);

public:
    CameraTrackController();
    CameraTrackController(SharedPtr<CameraTrack> cameraTrack);
    virtual ~CameraTrackController() = default;

    const SharedPtr<CameraTrack>& GetCameraTrack() const
    {
        return m_cameraTrack;
    }

    void SetCameraTrack(SharedPtr<CameraTrack> cameraTrack)
    {
        m_cameraTrack = std::move(cameraTrack);
    }

    virtual void UpdateLogic(double delta) override;

protected:
    SharedPtr<CameraTrack> m_cameraTrack;
    double m_trackTime;
};

} // namespace Hyperion
