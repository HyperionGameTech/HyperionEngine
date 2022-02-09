#ifndef CAMERA_FOLLOW_CONTROL_H
#define CAMERA_FOLLOW_CONTROL_H

#include <cstddef>
#include <vector>

#include "../control.h"
#include "../rendering/camera/camera.h"
#include "../entity.h"

namespace hyperion {
class CameraFollowControl : public EntityControl {
public:
    CameraFollowControl(Camera *camera, const Vector3 &offset = Vector3::Zero());
    virtual ~CameraFollowControl();

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(double dt);

    inline const Vector3 &GetOffset() const { return m_offset; }
    inline void SetOffset(const Vector3 &offset) { m_offset = offset; }

private:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    Camera *m_camera;
    Vector3 m_offset;
};
} // namespace hyperion

#endif
