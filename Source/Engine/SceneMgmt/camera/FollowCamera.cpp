/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/camera/FollowCamera.hpp>

#include <FollowCamera.generated.inl>

namespace Hyperion {

FollowCameraController::FollowCameraController(const Vector3& target, const Vector3& offset)
    : PerspectiveCameraController(),
      m_target(target),
      m_offset(offset),
      m_realOffset(offset),
      m_mx(0.0f),
      m_my(0.0f),
      m_prevMx(0.0f),
      m_prevMy(0.0f),
      m_desiredDistance(target.Distance(offset))
{
}

void FollowCameraController::OnActivated()
{
    PerspectiveCameraController::OnActivated();

    m_camera->SetTarget(m_target);
}

void FollowCameraController::OnDeactivated()
{
    PerspectiveCameraController::OnDeactivated();
}

void FollowCameraController::UpdateLogic(double delta)
{
    m_realOffset.Lerp(m_offset, MathUtil::Clamp(float(delta) * 25.0f, 0.0f, 1.0f));

    const Vector3 origin = m_camera->GetTarget();
    const Vector3 normalizedOffsetDirection = (origin - (origin + m_realOffset)).Normalized();

    m_camera->SetWorldTranslation(origin + normalizedOffsetDirection * m_desiredDistance);
}

} // namespace Hyperion
