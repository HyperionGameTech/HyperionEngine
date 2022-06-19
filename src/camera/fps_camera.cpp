#include "fps_camera.h"

namespace hyperion {
FpsCamera::FpsCamera(int width, int height, float fov, float _near, float _far)
    : PerspectiveCamera(fov, width, height, _near, _far),
      m_mouse_x(0),
      m_mouse_y(0),
      m_prev_mouse_x(0),
      m_prev_mouse_y(0)
{
    m_dir_cross_y = Vector3(m_direction).Cross(m_up);
}

void FpsCamera::SetTranslation(const Vector3 &vec)
{
    Camera::SetTranslation(vec);
    m_next_translation = m_translation;
}

void FpsCamera::UpdateLogic(double dt)
{
    m_prev_mouse_x = m_mouse_x;
    m_prev_mouse_y = m_mouse_y;

    if constexpr (movement_blending > 0.0f) {
        m_translation.Lerp(
            m_next_translation,
            MathUtil::Clamp(
                (1.0f / movement_blending) * static_cast<float>(dt),
                0.0f,
                1.0f
            )
        );
    } else {
        m_translation = m_next_translation;
    }
}

void FpsCamera::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mouse_x = command.mag_data.mouse_x;
        m_mouse_y = command.mag_data.mouse_y;

        m_mag = {
            m_mouse_x - m_prev_mouse_x,
            m_mouse_y - m_prev_mouse_y
        };

        m_dir_cross_y = Vector3(m_direction).Cross(m_up);

        Rotate(m_up, MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
        Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

        if (m_direction.y > 0.97f || m_direction.y < -0.97f) {
            m_mag.y *= -1.0f;

            Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));
        }
    
        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        const float speed = movement_speed * static_cast<float>(dt);

        switch (command.movement_data.movement_type) {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_next_translation += m_direction * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_next_translation -= m_direction * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_next_translation -= m_dir_cross_y * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_next_translation += m_dir_cross_y * speed;

            break;
        }
    
        break;

    }

    default:
        break;
    }
}

} // namespace hyperion
