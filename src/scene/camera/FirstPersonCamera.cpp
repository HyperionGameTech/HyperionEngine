#include "FirstPersonCamera.hpp"

namespace hyperion::v2 {
FirstPersonCameraController::FirstPersonCameraController()
    : PerspectiveCameraController()
{
}

void FirstPersonCameraController::UpdateLogic(double dt)
{
    m_desired_mag = Vector2(
        m_mouse_x - m_prev_mouse_x,
        m_mouse_y - m_prev_mouse_y
    );
    
    m_mag.Lerp(m_desired_mag, mouse_blending);

    m_dir_cross_y = Vector3(m_camera->m_direction).Cross(m_camera->m_up);

    m_camera->Rotate(m_camera->m_up, MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
    m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

    if (m_camera->m_direction.y > 0.98f || m_camera->m_direction.y < -0.98f) {
        m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(-m_mag.y * mouse_sensitivity));
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
    } else {

        m_move_deltas.Lerp(
            Vector3::Zero(),
            MathUtil::Clamp(
                1.0f * static_cast<float>(dt),
                0.0f,
                1.0f
            )
        );
    }

    m_camera->m_next_translation += m_move_deltas * movement_speed * dt;
}

void FirstPersonCameraController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
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
            m_move_deltas += m_camera->m_direction;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_move_deltas -= m_camera->m_direction;// * speed;

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
