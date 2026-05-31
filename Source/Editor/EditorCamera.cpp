/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <Core/config/Config.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <system/AppContext.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#include <EditorCamera.generated.inl>

namespace Hyperion {

static constexpr double LookSensitivity = 25.0;
static constexpr double MovementSpeed = 10.0;

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Camera);

template <class AllocatorType>
static TBitset<AllocatorType> GetMovementKeys(bool includeArrowKeys = true)
{
    TBitset<AllocatorType> bits;
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

// Max key enum value (KEY_W) < (32*4)
static const auto s_movementKeys = GetMovementKeys<FixedAllocator<(128 / Bitset::NumBitsPerBlock)>>(true);

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

    Camera* camera = m_controller->GetCamera();
    Assert(camera != nullptr);

    const double lookMultiplier = LookSensitivity;
    const double moveMultiplier = MovementSpeed;

    const double mouseDeltaX = double(evt.relativePos.x) - double(evt.relativePrevPos.x);
    const double mouseDeltaY = double(evt.relativePos.y) - double(evt.relativePrevPos.y);

    const Vec3f dirCrossY = camera->GetSideVector();

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
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(-mouseDeltaX * lookMultiplier));
        camera->Rotate(dirCrossY, MathUtil::DegToRad(-mouseDeltaY * lookMultiplier));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
        {
            camera->Rotate(dirCrossY, MathUtil::DegToRad(mouseDeltaY * lookMultiplier));
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
