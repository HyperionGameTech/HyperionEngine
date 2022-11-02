#ifndef HYPERION_V2_FIRST_PERSON_CAMERA_H
#define HYPERION_V2_FIRST_PERSON_CAMERA_H

#include <math/Vector2.hpp>
#include "PerspectiveCamera.hpp"

namespace hyperion::v2 {
class FirstPersonCamera : public PerspectiveCamera {
public:
    static constexpr float mouse_sensitivity = 1.0f;
    static constexpr float mouse_blending = 0.25f;
    static constexpr float movement_speed = 5.0f;
    static constexpr float movement_speed_2 = movement_speed * 2.0f;
    static constexpr float movement_blending = 0.3f;

    FirstPersonCamera(int width, int height, float fov, float _near, float _far);
    virtual ~FirstPersonCamera() = default;

    virtual void SetTranslation(const Vector3 &translation) override;
    virtual void SetNextTranslation(const Vector3 &translation) override;

    virtual void UpdateLogic(double dt) override;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    Vector3 m_move_deltas,
            m_dir_cross_y;

    float m_mouse_x,
          m_mouse_y,
          m_prev_mouse_x,
          m_prev_mouse_y;
    
    Vector2 m_mag,
            m_desired_mag,
            m_prev_mag;
};
} // namespace hyperion::v2

#endif
