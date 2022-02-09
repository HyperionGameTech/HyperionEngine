#include "camera_follow_control.h"

namespace hyperion {
CameraFollowControl::CameraFollowControl(Camera *camera, const Vector3 &offset)
    : EntityControl(fbom::FBOMObjectType("CAMERA_FOLLOW_CONTROL"), 60.0),
      m_camera(camera),
      m_offset(offset)
{
}

CameraFollowControl::~CameraFollowControl()
{
}

void CameraFollowControl::OnAdded()
{
}

void CameraFollowControl::OnRemoved()
{
}

void CameraFollowControl::OnUpdate(double dt)
{
    if (m_camera == nullptr) {
        return;
    }

    parent->SetGlobalTranslation(m_camera->GetTranslation() + m_offset);
}

std::shared_ptr<EntityControl> CameraFollowControl::CloneImpl()
{
    return std::make_shared<CameraFollowControl>(nullptr, m_offset); // todo
}
} // namespace hyperion
