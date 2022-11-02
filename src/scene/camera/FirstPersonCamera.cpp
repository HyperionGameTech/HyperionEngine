#include "FirstPersonCamera.hpp"

namespace hyperion::v2 {
FirstPersonCamera::FirstPersonCamera(int width, int height, float fov, float _near, float _far)
    : PerspectiveCamera(fov, width, height, _near, _far),
      m_mouse_x(0),
      m_mouse_y(0),
      m_prev_mouse_x(0),
      m_prev_mouse_y(0)
{
    m_dir_cross_y = Vector3(m_direction).Cross(m_up);
}

void FirstPersonCamera::SetTranslation(const Vector3 &translation)
{
    Camera::SetTranslation(translation);

    // m_move_translation = translation;
}

void FirstPersonCamera::SetNextTranslation(const Vector3 &translation)
{
    Camera::SetNextTranslation(translation);

    // m_move_translation = translation;
}

void FirstPersonCamera::UpdateLogic(double dt)
{
    m_desired_mag = Vector2(
        m_mouse_x - m_prev_mouse_x,
        m_mouse_y - m_prev_mouse_y
    );
    
    m_mag.Lerp(m_desired_mag, mouse_blending);

    m_dir_cross_y = Vector3(m_direction).Cross(m_up);

    Rotate(m_up, MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
    Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

    if (m_direction.y > 0.98f || m_direction.y < -0.98f) {
        Rotate(m_dir_cross_y, MathUtil::DegToRad(-m_mag.y * mouse_sensitivity));
    }

    m_prev_mouse_x = m_mouse_x;
    m_prev_mouse_y = m_mouse_y;

    if constexpr (movement_blending > 0.0f) {
        m_move_deltas.Lerp(
            Vector3::Zero(),
            MathUtil::Clamp(
                (1.0f / movement_blending) * static_cast<float>(dt),
                0.0f,
                1.0f
            )
        );


        // m_next_translation.Lerp(
        //     m_move_translation,
        //     MathUtil::Clamp(
        //         (1.0f / movement_blending) * static_cast<float>(dt),
        //         0.0f,
        //         1.0f
        //     )
        // );
    } else {

        m_move_deltas.Lerp(
            Vector3::Zero(),
            MathUtil::Clamp(
                1.0f * static_cast<float>(dt),
                0.0f,
                1.0f
            )
        );

        // m_next_translation.Lerp(
        //     m_move_translation,
        //     MathUtil::Clamp(
        //         1.0f * static_cast<float>(dt),
        //         0.0f,
        //         1.0f
        //     )
        // );
    }

    m_next_translation += m_move_deltas * movement_speed * dt;
}

void FirstPersonCamera::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mouse_x = command.mag_data.mouse_x;
        m_mouse_y = command.mag_data.mouse_y;
    
        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        const float speed = movement_speed;// * static_cast<float>(dt);

        switch (command.movement_data.movement_type) {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_move_deltas += m_direction;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_move_deltas -= m_direction;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_move_deltas -= m_dir_cross_y;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_move_deltas += m_dir_cross_y;// * speed;

            break;
        }
    
        break;

    }

    default:
        break;
    }
}

} // namespace hyperion::v2
