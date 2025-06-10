/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Camera);

#pragma region EditorCameraInputHandler

EditorCameraInputHandler::EditorCameraInputHandler(CameraController* controller)
    : m_controller(dynamic_cast<EditorCameraController*>(controller))
{
    AssertThrowMsg(m_controller != nullptr, "Null camera controller or not of type EditorCameraController");
}

bool EditorCameraInputHandler::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    return InputHandlerBase::OnKeyDown_Impl(evt);
}

bool EditorCameraInputHandler::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    return InputHandlerBase::OnKeyUp_Impl(evt);
}

bool EditorCameraInputHandler::OnMouseDown_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    m_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);

    return true;
}

bool EditorCameraInputHandler::OnMouseUp_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

    return true;
}

bool EditorCameraInputHandler::OnMouseMove_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    return false;
}

bool EditorCameraInputHandler::OnMouseDrag_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    const Vec2f delta = (evt.position - evt.previous_position) * 150.0f;

    const Vec3f dir_cross_y = camera->GetDirection().Cross(camera->GetUpVector());

    if (evt.mouse_buttons & MouseButtonState::LEFT)
    {
        const Vec2i delta_sign = Vec2i(MathUtil::Sign(delta));

        if (evt.mouse_buttons & MouseButtonState::RIGHT)
        {
            camera->SetNextTranslation(camera->GetTranslation() + dir_cross_y * 0.1f * float(-delta_sign.y));
        }
        else
        {
            camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));

            camera->SetNextTranslation(camera->GetTranslation() + camera->GetDirection() * 0.1f * float(-delta_sign.y));
        }
    }
    else if (evt.mouse_buttons & MouseButtonState::RIGHT)
    {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
        camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
        }
    }

    return true;
}

bool EditorCameraInputHandler::OnClick_Impl(const MouseEvent& evt)
{
    return false;
}

#pragma endregion EditorCameraInputHandler

#pragma region EditorCameraController

EditorCameraController::EditorCameraController()
    : FirstPersonCameraController(),
      m_mode(EditorCameraControllerMode::INACTIVE)
{
    m_input_handler = CreateObject<EditorCameraInputHandler>(this);
    InitObject(m_input_handler);
}

void EditorCameraController::OnActivated()
{
    HYP_SCOPE;

    FirstPersonCameraController::OnActivated();
}

void EditorCameraController::SetMode(EditorCameraControllerMode mode)
{
    HYP_SCOPE;

    switch (mode)
    {
    case EditorCameraControllerMode::INACTIVE:
    case EditorCameraControllerMode::FOCUSED: // fallthrough
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);

        break;
    case EditorCameraControllerMode::MOUSE_LOCKED:
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

        break;
    }

    m_mode = mode;
}

void EditorCameraController::UpdateLogic(double delta)
{
    HYP_SCOPE;

    FirstPersonCameraController::UpdateLogic(delta);

    static constexpr float speed = 15.0f;

    Vec3f translation = m_camera->GetTranslation();

    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dir_cross_y = direction.Cross(m_camera->GetUpVector());

    if (m_input_handler->IsKeyDown(KeyCode::KEY_W))
    {
        translation += direction * delta * speed;
    }
    if (m_input_handler->IsKeyDown(KeyCode::KEY_S))
    {
        translation -= direction * delta * speed;
    }
    if (m_input_handler->IsKeyDown(KeyCode::KEY_A))
    {
        translation -= dir_cross_y * delta * speed;
    }
    if (m_input_handler->IsKeyDown(KeyCode::KEY_D))
    {
        translation += dir_cross_y * delta * speed;
    }

    m_camera->SetNextTranslation(translation);
}

void EditorCameraController::RespondToCommand(const CameraCommand& command, GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    switch (command.command)
    {
    case CameraCommand::CAMERA_COMMAND_MAG:
    case CameraCommand::CAMERA_COMMAND_MOVEMENT: // fallthrough
        if (m_mode == EditorCameraControllerMode::INACTIVE)
        {
            // Don't handle command
            return;
        }

        // Let base class handle command
        break;

    default:
        break;
    }

    FirstPersonCameraController::RespondToCommand(command, dt);
}

#pragma endregion EditorCameraController

} // namespace hyperion
