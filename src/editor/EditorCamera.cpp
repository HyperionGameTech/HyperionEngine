/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <core/config/Config.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <system/AppContext.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_API extern const GlobalConfig& GetGlobalConfig();

HYP_DECLARE_LOG_CHANNEL(Camera);

static Bitset CreateWasdBitset(bool includeArrowKeys = true)
{
    Bitset bits;
    bits.Set(uint32(KeyCode::KEY_W), true);
    bits.Set(uint32(KeyCode::KEY_A), true);
    bits.Set(uint32(KeyCode::KEY_S), true);
    bits.Set(uint32(KeyCode::KEY_D), true);

    if (includeArrowKeys)
    {
        bits.Set(uint32(KeyCode::ARROW_LEFT), true);
        bits.Set(uint32(KeyCode::ARROW_RIGHT), true);
        bits.Set(uint32(KeyCode::ARROW_UP), true);
        bits.Set(uint32(KeyCode::ARROW_DOWN), true);
    }

    return bits;
}

static const Bitset g_wasdBits = CreateWasdBitset(true);

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
        if (!(evt.mouseButtons & (MouseButtonState::LEFT | MouseButtonState::RIGHT)))
        {
            controller->SetMode(EditorCameraControllerMode::INACTIVE);
        }

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
    
    static const ConfigurationValue& editorLookSensitivity = GetGlobalConfig().Get("editor.camera.lookSensitivity");
    static const ConfigurationValue& editorMoveSensitivity = GetGlobalConfig().Get("editor.camera.lookSensitivity");

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
    
    // magic numbers for fun
    const float lookMultiplier = 5000.0f * editorLookSensitivity.ToFloat(1.0f);
    const float moveMultiplier = 250.0f * editorMoveSensitivity.ToFloat(1.0f);

    const Vec2f mouseDelta = (evt.position - evt.previousPosition) * m_deltaTime;
    const Vec2f deltaSign = Vec2f(MathUtil::Sign(evt.position - evt.previousPosition)) * m_deltaTime;

    const Vec3f dirCrossY = camera->GetDirection().Cross(camera->GetUpVector());

    const bool isAltPressed = IsKeyDown(KeyCode::LEFT_ALT) || IsKeyDown(KeyCode::RIGHT_ALT);
    const bool isMoveKeyPressed = (GetKeyStates() & g_wasdBits).Count() != 0;

    if (isAltPressed || (evt.mouseButtons & (MouseButtonState::LEFT & MouseButtonState::RIGHT)))
    {
        // Get the forward vector on the horizontal plane
        Vec3f forward = camera->GetDirection();
        forward.y = 0.0f; // Ignore vertical component
        forward.Normalize();

        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(mouseDelta.x) * lookMultiplier);

        if (!isMoveKeyPressed)
        {
            camera->SetTranslation(camera->GetTranslation() + forward * -deltaSign.y * moveMultiplier);
        }
    }
    else if ((evt.mouseButtons & MouseButtonState::RIGHT))
    {
        if (!isMoveKeyPressed) // so we don't try to move using cam when any movement keys are pressed. would be annoying.
        {
            if (MathUtil::Abs(mouseDelta.y) > MathUtil::Abs(mouseDelta.x))
            {
                camera->SetTranslation(camera->GetTranslation() + camera->GetUpVector() * -deltaSign.y * moveMultiplier);
            }
            else
            {
                camera->SetTranslation(camera->GetTranslation() + dirCrossY * deltaSign.x * moveMultiplier);
            }
        }
    }
    else if (evt.mouseButtons & MouseButtonState::LEFT)
    {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(mouseDelta.x) * lookMultiplier);
        camera->Rotate(dirCrossY, MathUtil::DegToRad(mouseDelta.y) * lookMultiplier);

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dirCrossY, MathUtil::DegToRad(-mouseDelta.y) * lookMultiplier);
        }
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseLeave_Impl(const MouseEvent& evt)
{
    if (Handle<EditorCameraController> controller = m_controller.Lock())
    {
        controller->SetMode(EditorCameraControllerMode::INACTIVE);

        return true;
    }
    
    return false;
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
    
    m_inputHandler->SetDeltaTime(delta);

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
