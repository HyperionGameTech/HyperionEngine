#include "FirstPersonCamera.hpp"

namespace hyperion::v2 {

static const float mouse_sensitivity = 1.0f;
static const float mouse_blending = 0.35f;
static const float movement_speed = 5.0f;
static const float movement_speed_2 = movement_speed * 2.0f;
static const float movement_blending = 0.01f;

FirstPersonCameraController::FirstPersonCameraController(FirstPersonCameraControllerMode mode)
    : PerspectiveCameraController(),
      m_mode(mode),
      m_mouse_x(0.0f),
      m_mouse_y(0.0f),
      m_prev_mouse_x(0.0f),
      m_prev_mouse_y(0.0f)
{
}

void FirstPersonCameraController::UpdateLogic(double dt)
{
    switch (m_mode) {
    case FPC_MODE_MOUSE_LOCKED:
        m_desired_mag = Vec2f {
            m_mouse_x - (float(m_camera->GetWidth()) / 2.0f),
            m_mouse_y - (float(m_camera->GetHeight()) / 2.0f)
        };
        break;
    case FPC_MODE_MOUSE_FREE:
        m_desired_mag = Vec2f {
            m_mouse_x - m_prev_mouse_x,
            m_mouse_y - m_prev_mouse_y
        };

        break;
    }
    
    
    m_mag.Lerp(m_desired_mag, MathUtil::Min(1.0f, 1.0f - mouse_blending));

    m_dir_cross_y = m_camera->GetDirection().Cross(m_camera->GetUpVector());

    m_camera->Rotate(m_camera->GetUpVector(), MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
    m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

    if (m_camera->GetDirection().y > 0.98f || m_camera->GetDirection().y < -0.98f) {
        m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(-m_mag.y * mouse_sensitivity));
    }

    m_prev_mouse_x = m_mouse_x;
    m_prev_mouse_y = m_mouse_y;

    m_camera->m_next_translation += m_move_deltas * movement_speed * float(dt);

    if (movement_blending > 0.0f) {
        m_move_deltas.Lerp(
            Vector3::Zero(),
            MathUtil::Clamp(
                (1.0f / movement_blending) * float(dt),
                0.0f,
                1.0f
            )
        );
    } else {
        m_move_deltas.Lerp(
            Vector3::Zero(),
            MathUtil::Clamp(
                1.0f * float(dt),
                0.0f,
                1.0f
            )
        );
    }
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
