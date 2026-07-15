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
    FollowCameraController()
        : FollowCameraController(Vec3f::Zero(), Vec3f(0.0f, 0.0f, -4.0f))
    {
    }

    FollowCameraController(const Vec3f& target, const Vec3f& offset);
    virtual ~FollowCameraController() override = default;

    HYP_FORCE_INLINE const Vector3& GetOffset() const
    {
        return m_offset;
    }

    HYP_FORCE_INLINE void SetOffset(const Vector3& offset)
    {
        m_offset = offset;
    }

    virtual void UpdateLogic(double delta) override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

private:
    Vec3f m_offset;
    Vec3f m_realOffset;

    Vec3f m_target;

    float m_mx;
    float m_my;
    float m_prevMx;
    float m_prevMy;
    float m_desiredDistance;

    Vec2f m_mag;
    Vec2f m_prevMag;
};

} // namespace Hyperion
