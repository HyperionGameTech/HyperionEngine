/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <Core/config/Config.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <system/AppContext.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/config/EngineConfig.hpp>

#include <EditorCamera.generated.inl>

namespace Hyperion {

extern EngineConfig& GetEngineConfig();

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

bool EditorCameraInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyDown(evt);

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

bool EditorCameraInputHandler::OnKeyUp(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyUp(evt);

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

bool EditorCameraInputHandler::OnMouseDown(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseDown(evt);

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

bool EditorCameraInputHandler::OnMouseUp(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseUp(evt);

    if (!m_controller)
    {
        return false;
    }

    if (!((GetMouseButtonStates() & (MouseButtonState::LEFT | MouseButtonState::RIGHT))))
    {
        m_controller->SetMode(EditorCameraControllerMode::INACTIVE);
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseMove(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnMouseDrag(const MouseEvent& evt)
{
    HYP_SCOPE;

    const ConfigValue& editorLookSensitivity = GetEngineConfig().Get("Editor.Camera.LookSensitivity");
    const ConfigValue& editorMoveSensitivity = GetEngineConfig().Get("Editor.Camera.MoveSensitivity");

    Camera* camera = m_controller->GetCamera();
    Assert(camera != nullptr);

    const double lookMultiplier = 7.5 * editorLookSensitivity.ToDouble(1.0);
    const double moveMultiplier = 24.0 * editorMoveSensitivity.ToDouble(1.0);

    const double mouseDeltaX = double(evt.relativePos.x) - double(evt.relativePrevPos.x);
    const double mouseDeltaY = double(evt.relativePos.y) - double(evt.relativePrevPos.y);

    const Vec3f dirCrossY = camera->GetDirection().Cross(camera->GetUpVector());

    const bool isAltPressed = IsKeyDown(KeyCode::KEY_LALT) || IsKeyDown(KeyCode::KEY_RALT);
    const bool isCtrlPressed = IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL);
    const bool isMoveKeyPressed = (GetKeyStates() & s_movementKeys).Count() != 0;

    constexpr EnumFlags<MouseButtonState> ButtonsLR = MouseButtonState::LEFT | MouseButtonState::RIGHT;

    if (isAltPressed || (evt.mouseButtons & ButtonsLR) == ButtonsLR)
    {
        if (!isMoveKeyPressed)
        {
            Vec3f translationDelta = (dirCrossY * float(mouseDeltaX)) + (camera->GetUpVector() * float(-mouseDeltaY));
            camera->SetWorldTranslation(camera->GetWorldTranslation() + (translationDelta * moveMultiplier * float(m_deltaTime)));
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
            Vec3f translationDelta = (dirCrossY * float(mouseDeltaX)) + (forward * float(-mouseDeltaY));
            camera->SetWorldTranslation(camera->GetWorldTranslation() + (translationDelta * moveMultiplier * float(m_deltaTime)));
        }
    }
    else if (evt.mouseButtons & MouseButtonState::LEFT)
    {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(mouseDeltaX * lookMultiplier));
        camera->Rotate(dirCrossY, MathUtil::DegToRad(mouseDeltaY * lookMultiplier));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dirCrossY, MathUtil::DegToRad(-mouseDeltaY * lookMultiplier));
        }
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseLeave(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseLeave(evt);

    if (!m_controller)
    {
        return false;
    }

    m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

    return true;
}

bool EditorCameraInputHandler::OnClick(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnGainFocus(const MouseEvent& evt)
{
    return false;
}

bool EditorCameraInputHandler::OnLoseFocus(const MouseEvent& evt)
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
    m_inputHandler = MakeHandle<EditorCameraInputHandler>(this);
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

} // namespace Hyperion
