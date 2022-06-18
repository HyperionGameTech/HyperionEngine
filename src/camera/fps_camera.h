#ifndef FPS_CAMERA_H
#define FPS_CAMERA_H

#include <math/vector2.h>
#include "perspective_camera.h"

namespace hyperion {
class FpsCamera : public PerspectiveCamera {
public:
    static constexpr float mouse_sensitivity = 1.0f;
    static constexpr float movement_speed = 1000.0f;
    static constexpr float movement_speed_2 = movement_speed * 2.0f;
    static constexpr float movement_blending = 0.3f;

    FpsCamera(int width, int height, float fov, float _near, float _far);
    virtual ~FpsCamera() = default;

    virtual void SetTranslation(const Vector3 &vec) override;
    virtual void UpdateLogic(double dt) override;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    Vector3 m_dir_cross_y;
    Vector3 m_next_translation;

    float m_mouse_x,
          m_mouse_y,
          m_prev_mouse_x,
          m_prev_mouse_y;
    
    Vector2 m_mag,
            m_prev_mag;
};
} // namespace hyperion

#endif
