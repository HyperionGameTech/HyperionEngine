/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <core/config/Config.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <system/AppContext.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

#include <EditorCamera.generated.inl>

namespace hyperion {

extern const GlobalConfig& CoreApi_GetGlobalConfig();

HYP_DECLARE_LOG_CHANNEL(Camera);

static Bitset GetMovementKeys(bool includeArrowKeys = true)
{
    Bitset bits;
    bits.Set(uint32(KeyCode::KEY_W), true);
    bits.Set(uint32(KeyCode::KEY_A), true);
    bits.Set(uint32(KeyCode::KEY_S), true);
    bits.Set(uint32(KeyCode::KEY_D), true);

    if (includeArrowKeys)
    {
        bits.Set(uint32(KeyCode::KEY_LEFT), true);
        bits.Set(uint32(KeyCode::KEY_RIGHT), true);
        bits.Set(uint32(KeyCode::KEY_UP), true);
        bits.Set(uint32(KeyCode::KEY_DOWN), true);
    }

    return bits;
}

static const Bitset s_movementKeys = GetMovementKeys(true);

#pragma region EditorCameraInputHandler

EditorCameraInputHandler::EditorCameraInputHandler()
    : m_controller(nullptr)
{
}

EditorCameraInputHandler::EditorCameraInputHandler(EditorCameraController* controller)
    : m_controller(controller)
{
}

bool EditorCameraInputHandler::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyDown_Impl(evt);

    if (m_controller && m_controller->GetMode() == EditorCameraControllerMode::MOUSE_LOCKED)
    {
        if (evt.keyCode == KeyCode::KEY_ESCAPE)
        {
            m_controller->SetMode(EditorCameraControllerMode::INACTIVE);
            return true;
        }

        return true;
    }

    return false;
}

bool EditorCameraInputHandler::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyUp_Impl(evt);

    if (m_controller && m_controller->GetMode() == EditorCameraControllerMode::MOUSE_LOCKED)
    {
        if (evt.keyCode == KeyCode::KEY_ESCAPE)
        {
            m_controller->SetMode(EditorCameraControllerMode::INACTIVE);
            return true;
        }

        return true;
    }

    return false;
}

bool EditorCameraInputHandler::OnMouseDown_Impl(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseDown_Impl(evt);

    if (!m_controller)
    {
        return false;
    }

    if (evt.mouseButtons & (MouseButtonState::LEFT | MouseButtonState::RIGHT))
    {
        m_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseUp_Impl(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseUp_Impl(evt);

    if (!m_controller)
    {
        return false;
    }

    if (!IsKeyDown(KeyCode::KEY_LEFT) && !IsKeyDown(KeyCode::KEY_RIGHT))
    {
        m_controller->SetMode(EditorCameraControllerMode::INACTIVE);
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseMove_Impl(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnMouseDrag_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    static const ConfigurationValue& s_editorLookSensitivity = CoreApi_GetGlobalConfig().Get("Editor.Camera.LookSensitivity");
    static const ConfigurationValue& s_editorMoveSensitivity = CoreApi_GetGlobalConfig().Get("Editor.Camera.MoveSensitivity");

    if (!m_controller)
    {
        return false;
    }

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    // magic numbers for fun
    const double lookMultiplier = 7000.0 * s_editorLookSensitivity.ToDouble(1.0);
    const double moveMultiplier = 25.0 * s_editorMoveSensitivity.ToDouble(1.0);

    // double for moar bits!!1
    const double mouseDeltaX = double(evt.relativePos.x) - double(evt.relativePrevPos.x);
    const double mouseDeltaY = double(evt.relativePos.y) - double(evt.relativePrevPos.y);

    const Vec2f deltaSign = Vec2f(MathUtil::Sign(evt.relativePos - evt.relativePrevPos)) * float(m_deltaTime);

    const Vec3f dirCrossY = camera->GetDirection().Cross(camera->GetUpVector());

    const bool isAltPressed = IsKeyDown(KeyCode::KEY_LALT) || IsKeyDown(KeyCode::KEY_RALT);
    const bool isCtrlPressed = IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL);
    const bool isMoveKeyPressed = (GetKeyStates() & s_movementKeys).Count() != 0;

    constexpr EnumFlags<MouseButtonState> ButtonsLR = MouseButtonState::LEFT | MouseButtonState::RIGHT;

    if (isAltPressed || (evt.mouseButtons & ButtonsLR) == ButtonsLR)
    {
        if (!isMoveKeyPressed) // so we don't try to move using cam when any movement keys are pressed. would be annoying.
        {
            if (MathUtil::Abs(mouseDeltaY) > MathUtil::Abs(mouseDeltaX))
            {
                camera->SetTranslation(camera->GetTranslation() + camera->GetUpVector() * -deltaSign.y * moveMultiplier);
            }
            else
            {
                camera->SetTranslation(camera->GetTranslation() + dirCrossY * deltaSign.x * moveMultiplier);
            }
        }
    }
    else if ((evt.mouseButtons & MouseButtonState::RIGHT))
    {
        Vec3f forward = camera->GetDirection();
        forward.y = 0.0f;
        forward.Normalize();

        if (isCtrlPressed)
        {
            // rotate around the focal point
        }
        else if (!isMoveKeyPressed)
        {
            if (MathUtil::Abs(mouseDeltaY) > MathUtil::Abs(mouseDeltaX))
            {
                camera->SetTranslation(camera->GetTranslation() + forward * -deltaSign.y * moveMultiplier);
            }
            else
            {
                camera->SetTranslation(camera->GetTranslation() + dirCrossY * deltaSign.x * moveMultiplier);
            }
        }
    }
    else if (evt.mouseButtons & MouseButtonState::LEFT)
    {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(mouseDeltaX * lookMultiplier * m_deltaTime));
        camera->Rotate(dirCrossY, MathUtil::DegToRad(mouseDeltaY * lookMultiplier * m_deltaTime));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dirCrossY, MathUtil::DegToRad(-mouseDeltaY * lookMultiplier * m_deltaTime));
        }
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseLeave_Impl(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseLeave_Impl(evt);

    if (!m_controller)
    {
        return false;
    }

    m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

    return true;
}

bool EditorCameraInputHandler::OnClick_Impl(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnGainFocus_Impl(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnLoseFocus_Impl(const MouseEvent& evt)
{
    if (!m_controller)
    {
        return false;
    }

    m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

    return false;
}

#pragma endregion EditorCameraInputHandler

#pragma region EditorCameraController

EditorCameraController::EditorCameraController()
    : FirstPersonCameraController(),
      m_mode(EditorCameraControllerMode::INACTIVE)
{
    m_inputHandler = CreateObject<EditorCameraInputHandler>(this);
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
}

#pragma endregion EditorCameraController

} // namespace hyperion
