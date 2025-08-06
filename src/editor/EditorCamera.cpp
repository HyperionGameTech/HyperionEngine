/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <system/AppContext.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Camera);

#pragma region EditorCameraInputHandler

EditorCameraInputHandler::EditorCameraInputHandler(const WeakHandle<CameraController>& controller)
    : m_controller(controller)
{
    Assert(m_controller.IsValid(), "Null camera controller or not of type EditorCameraController");
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

    if (Handle<EditorCameraController> controller = m_controller.Lock())
    {
        controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);

        return true;
    }

    return false;
}

bool EditorCameraInputHandler::OnMouseUp_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    if (Handle<EditorCameraController> controller = m_controller.Lock())
    {
        controller->SetMode(EditorCameraControllerMode::INACTIVE);

        return true;
    }

    return false;
}

bool EditorCameraInputHandler::OnMouseMove_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    return false;
}

bool EditorCameraInputHandler::OnMouseDrag_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    Handle<EditorCameraController> controller = m_controller.Lock();

    if (!controller.IsValid())
    {
        return false;
    }

    Camera* camera = controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    constexpr float lookMultiplier = 30.0f;
    constexpr float moveMultiplier = 0.16f;

    const Vec2f delta = (evt.position - evt.previousPosition);

    const Vec3f dirCrossY = camera->GetDirection().Cross(camera->GetUpVector());

    if (evt.mouseButtons & MouseButtonState::RIGHT)
    {
        const Vec2i deltaSign = Vec2i(MathUtil::Sign(delta));

        if (evt.mouseButtons & MouseButtonState::LEFT)
        {
            if (MathUtil::Abs(delta.y) > MathUtil::Abs(delta.x))
            {
                camera->SetTranslation(camera->GetTranslation() + camera->GetUpVector() * float(-deltaSign.y) * moveMultiplier);
            }
            else
            {
                camera->SetTranslation(camera->GetTranslation() + dirCrossY * float(deltaSign.x) * moveMultiplier);
            }
        }
        else
        {
            // Get the forward vector on the horizontal plane
            Vec3f forward = camera->GetDirection();
            forward.y = 0.0f; // Ignore vertical component
            forward.Normalize();

            camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x * lookMultiplier));

            camera->SetTranslation(camera->GetTranslation() + forward * float(-deltaSign.y) * moveMultiplier);
        }
    }
    else if (evt.mouseButtons & MouseButtonState::LEFT)
    {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x * lookMultiplier));
        camera->Rotate(dirCrossY, MathUtil::DegToRad(delta.y * lookMultiplier));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dirCrossY, MathUtil::DegToRad(-delta.y * lookMultiplier));
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
    m_inputHandler = CreateObject<EditorCameraInputHandler>(WeakHandleFromThis());
    InitObject(m_inputHandler);
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
    const Vec3f dirCrossY = direction.Cross(m_camera->GetUpVector());

    if (m_inputHandler->IsKeyDown(KeyCode::KEY_W))
    {
        translation += direction * delta * speed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_S))
    {
        translation -= direction * delta * speed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_A))
    {
        translation -= dirCrossY * delta * speed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_D))
    {
        translation += dirCrossY * delta * speed;
    }

    m_camera->SetNextTranslation(translation);
}

void EditorCameraController::RespondToCommand(const CameraCommand& command, float dt)
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
