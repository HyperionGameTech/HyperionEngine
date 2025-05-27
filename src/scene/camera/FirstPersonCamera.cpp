/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/FirstPersonCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

static const float mouse_sensitivity = 1.0f;
static const float mouse_blending = 0.35f;
static const float movement_speed = 5.0f;
static const float movement_speed_2 = movement_speed * 2.0f;
static const float movement_blending = 0.01f;

#pragma region FirstPersonCameraInputHandler

FirstPersonCameraInputHandler::FirstPersonCameraInputHandler(CameraController* controller)
    : m_controller(dynamic_cast<FirstPersonCameraController*>(controller))
{
    AssertThrowMsg(m_controller != nullptr, "Null camera controller or not of type FirstPersonCameraInputHandler");
}

bool FirstPersonCameraInputHandler::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    return InputHandlerBase::OnKeyDown_Impl(evt);
}

bool FirstPersonCameraInputHandler::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    return InputHandlerBase::OnKeyUp_Impl(evt);
}

bool FirstPersonCameraInputHandler::OnMouseDown_Impl(const MouseEvent& evt)
{
    return InputHandlerBase::OnMouseDown_Impl(evt);
}

bool FirstPersonCameraInputHandler::OnMouseUp_Impl(const MouseEvent& evt)
{
    return InputHandlerBase::OnMouseUp_Impl(evt);
}

bool FirstPersonCameraInputHandler::OnMouseMove_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    const Vec2f delta = (evt.position - evt.previous_position) * 150.0f;

    const Vec3f dir_cross_y = camera->GetDirection().Cross(camera->GetUpVector());

    camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
    camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

    if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
    {
        camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
    }

    return true;
}

bool FirstPersonCameraInputHandler::OnMouseDrag_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    return false;
}

bool FirstPersonCameraInputHandler::OnClick_Impl(const MouseEvent& evt)
{
    return false;
}

#pragma endregion FirstPersonCameraInputHandler

#pragma region FirstPersonCameraController

FirstPersonCameraController::FirstPersonCameraController(FirstPersonCameraControllerMode mode)
    : PerspectiveCameraController(),
      m_mode(mode),
      m_mouse_x(0.0f),
      m_mouse_y(0.0f),
      m_prev_mouse_x(0.0f),
      m_prev_mouse_y(0.0f)
{
    m_input_handler = MakeRefCountedPtr<FirstPersonCameraInputHandler>(this);
}

void FirstPersonCameraController::OnActivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnActivated();
}

void FirstPersonCameraController::OnDeactivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnDeactivated();
}

void FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode mode)
{
    HYP_SCOPE;

    switch (mode)
    {
    case FirstPersonCameraControllerMode::MOUSE_FREE:
        CameraController::SetIsMouseLockRequested(false);

        break;
    case FirstPersonCameraControllerMode::MOUSE_LOCKED:
        CameraController::SetIsMouseLockRequested(true);

        break;
    }

    m_mode = mode;
}

void FirstPersonCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;
}

void FirstPersonCameraController::RespondToCommand(const CameraCommand& command, GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    switch (command.command)
    {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mouse_x = command.mag_data.mouse_x;
        m_mouse_y = command.mag_data.mouse_y;

        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        const float speed = movement_speed; // * static_cast<float>(dt);

        switch (command.movement_data.movement_type)
        {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_move_deltas += m_camera->m_direction; // * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_move_deltas -= m_camera->m_direction; // * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_move_deltas -= m_dir_cross_y; // * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_move_deltas += m_dir_cross_y; // * speed;

            break;
        default:
            break;
        }

        break;
    }

    default:
        break;
    }
}

#pragma endregion FirstPersonCameraController

} // namespace hyperion
