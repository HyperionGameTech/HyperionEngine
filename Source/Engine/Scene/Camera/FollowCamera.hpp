/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Camera/PerspectiveCamera.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API FollowCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(FollowCameraController);

public:
    FollowCameraController(const Vector3& target, const Vector3& offset);
    virtual ~FollowCameraController() override = default;

    const Vector3& GetOffset() const
    {
        return m_offset;
    }

    void SetOffset(const Vector3& offset)
    {
        m_offset = offset;
    }

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

private:
    Vec3f m_offset,
        m_realOffset;

    Vec3f m_target;

    float m_mx,
        m_my,
        m_prevMx,
        m_prevMy,
        m_desiredDistance;

    Vec2f m_mag,
        m_prevMag;
};

} // namespace Hyperion
