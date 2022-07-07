#ifndef FOLLOW_CAMERA_H
#define FOLLOW_CAMERA_H

#include "PerspectiveCamera.hpp"

namespace hyperion {
class FollowCamera : public PerspectiveCamera {
public:
    FollowCamera(
        const Vector3 &target, const Vector3 &offset,
        int width, int height,
        float fov,
        float _near, float _far
    );
    virtual ~FollowCamera() = default;

    const Vector3 &GetOffset() const      { return m_offset; }
    void SetOffset(const Vector3 &offset) { m_offset = offset; }

    virtual void UpdateLogic(double dt) override;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    Vector3 m_offset,
            m_real_offset;

    float m_mx,
          m_my,
          m_prev_mx,
          m_prev_my,
          m_desired_distance;
    
    Vector2 m_mag,
            m_prev_mag;
};
}

#endif
