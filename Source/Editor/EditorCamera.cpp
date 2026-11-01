/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorCamera.hpp>

#include <Input/InputManager.hpp>
#include <Input/InputHandler.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#include <Scene/Camera/CameraUtils.hpp>

#include <System/AppContext.hpp>

#include <EditorCamera.generated.inl>

namespace Hyperion {

static CVar<float> s_cvEditorCameraMouseSensitivity { "Editor.Camera.MouseSensitivity", 50.0f };
static CVar<float> s_cvEditorCameraMovementSpeed { "Editor.Camera.MovementSpeed", 0.5f };
static CVar<float> s_cvEditorCameraMoveSpeed { "Editor.Camera.MoveSpeed", 1.0f };

constexpr float BaseMoveSpeed = 15.0f;
constexpr float BoostMoveSpeedScale = 3.0f;
constexpr float ScrollMoveDistance = 1.0f;
constexpr float MoveSpeedScrollStep = 1.25f;

constexpr float MinMoveSpeed = 0.05f;
constexpr float MaxMoveSpeed = 50.0f;

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Camera);

static constexpr BitField<NumKeyboardKeys> GetMovementKeys(bool includeArrowKeys = true)
{
    BitField<NumKeyboardKeys> bits {};
    bits.Set(uint32(KeyCode::KEY_W), true);
    bits.Set(uint32(KeyCode::KEY_A), true);
    bits.Set(uint32(KeyCode::KEY_S), true);
    bits.Set(uint32(KeyCode::KEY_D), true);
    bits.Set(uint32(KeyCode::KEY_Q), true);
    bits.Set(uint32(KeyCode::KEY_E), true);

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
static constexpr BitField<NumKeyboardKeys> MovementKeys = GetMovementKeys(true);

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
    Camera* camera = m_controller->GetCamera();
    Assert(camera != nullptr);

    const double lookMultiplier = s_cvEditorCameraMouseSensitivity.Get();
    const double moveMultiplier = s_cvEditorCameraMovementSpeed.Get() * EditorCameraController::GetMoveSpeed();

    const double mouseDeltaX = double(evt.relativePos.x) - double(evt.relativePrevPos.x);
    const double mouseDeltaY = double(evt.relativePos.y) - double(evt.relativePrevPos.y);

    const Vec3f dirCrossY = camera->GetSideVector();

    const bool isAltPressed = IsKeyDown(KeyCode::KEY_LALT) || IsKeyDown(KeyCode::KEY_RALT);
    const bool isCtrlPressed = IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL);
    const bool isMoveKeyPressed = (GetKeyStates() & MovementKeys).CountOnes() != 0;

    constexpr EnumFlags<MouseButtonState> ButtonsLR = MouseButtonState::LEFT | MouseButtonState::RIGHT;

    ///ctrl can be used as RMB too
    ///so trackpads can have the same effect

    if (!isCtrlPressed && (isAltPressed || (evt.mouseButtons & ButtonsLR) == ButtonsLR))
    {
        if (!isMoveKeyPressed)
        {
            Vec3f translationDelta = (dirCrossY * float(mouseDeltaX)) + (camera->GetUpVector() * float(-mouseDeltaY));
            camera->SetWorldTranslation(camera->GetWorldTranslation() + (translationDelta * moveMultiplier));
        }
    }
    else if (isCtrlPressed || (evt.mouseButtons & MouseButtonState::RIGHT))
    {
        Vec3f forward = camera->GetDirection();
        forward.y = 0.0f;
        forward.Normalize();

        if (isCtrlPressed && (evt.mouseButtons & MouseButtonState::RIGHT))
        {
            // TODO: rotate around the focal point
        }
        else if (!isMoveKeyPressed)
        {
            Vec3f translationDelta = (dirCrossY * float(mouseDeltaX)) + (forward * float(-mouseDeltaY));
            camera->SetWorldTranslation(camera->GetWorldTranslation() + (translationDelta * moveMultiplier));
        }
    }
    else if (evt.mouseButtons & MouseButtonState::LEFT)
    {
        CameraUtils::ApplyYawPitch(
            *camera,
            MathUtil::DegToRad(mouseDeltaX * lookMultiplier),
            MathUtil::DegToRad(mouseDeltaY * lookMultiplier),
            0.98f);
    }

    return true;
}

bool EditorCameraInputHandler::OnMouseScroll(const MouseEvent& evt)
{
    if (!m_controller || evt.wheel.y == 0)
    {
        return false;
    }

    const float wheelSteps = float(evt.wheel.y) / 120.0f;

    ApplicationWindow* window = evt.baseEvent != nullptr ? evt.baseEvent->GetWindow() : nullptr;
    const InputManager* inputManager = window != nullptr ? window->GetInputManager().Get() : nullptr;

    bool changeSpeed = false;

    if (inputManager != nullptr)
    {
        changeSpeed = inputManager->IsCtrlDown();

        for (uint32 i = 0; i < NumKeyboardKeys && !changeSpeed; i++)
        {
            changeSpeed = MovementKeys.Test(i) && inputManager->IsKeyDown(KeyCode(i));
        }
    }
    else
    {
        changeSpeed = IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL)
            || (GetKeyStates() & MovementKeys).CountOnes() != 0;
    }

    if (changeSpeed)
    {
        EditorCameraController::SetMoveSpeed(EditorCameraController::GetMoveSpeed() * MathUtil::Pow(MoveSpeedScrollStep, wheelSteps));

        return true;
    }

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    camera->SetWorldTranslation(camera->GetWorldTranslation() + camera->GetDirection() * (wheelSteps * ScrollMoveDistance * EditorCameraController::GetMoveSpeed()));

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
    // Note: this also clears all tracked key / mouse button states
    InputHandlerBase::OnLoseFocus(evt);

    if (!m_controller)
    {
        return false;
    }

    // Exit mouse look mode when focus is lost, otherwise the mouse lock
    // would be re-applied when the window regains focus
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

float EditorCameraController::GetMoveSpeed()
{
    return s_cvEditorCameraMoveSpeed.Get();
}

void EditorCameraController::SetMoveSpeed(float moveSpeed)
{
    s_cvEditorCameraMoveSpeed.Set(MathUtil::Clamp(moveSpeed, MinMoveSpeed, MaxMoveSpeed));
}

void EditorCameraController::ResetMoveSpeed()
{
    SetMoveSpeed(1.0f);
}

Vec3f EditorCameraController::GetKeyboardMovement(double delta) const
{
    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dirCrossY = m_camera->GetSideVector();

    Vec3f movement;

    if (m_inputHandler->IsKeyDown(KeyCode::KEY_W))
    {
        movement += direction;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_S))
    {
        movement -= direction;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_A))
    {
        movement -= dirCrossY;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_D))
    {
        movement += dirCrossY;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_E))
    {
        movement += Vec3f::UnitY();
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_Q))
    {
        movement -= Vec3f::UnitY();
    }

    float speed = BaseMoveSpeed * GetMoveSpeed();

    if (m_inputHandler->IsKeyDown(KeyCode::KEY_LSHIFT) || m_inputHandler->IsKeyDown(KeyCode::KEY_RSHIFT))
    {
        speed *= BoostMoveSpeedScale;
    }

    return movement.Normalized() * float(delta) * speed;
}

void EditorCameraController::UpdateLogic(double delta)
{
    HYP_SCOPE;

    if (m_mode == EditorCameraControllerMode::MOUSE_LOCKED && g_appContext.IsValid() && g_appContext->GetMainWindow() != nullptr)
    {
        const EnumFlags<MouseButtonState> buttonStates = g_appContext->GetMainWindow()->GetInputManager()->GetButtonStates();

        if (!(buttonStates & (MouseButtonState::LEFT | MouseButtonState::RIGHT)))
        {
            SetMode(EditorCameraControllerMode::INACTIVE);
        }
    }

    FirstPersonCameraController::UpdateLogic(delta);
}

#pragma endregion EditorCameraController


#pragma region EditorOrthoCameraInputHandler

static constexpr float OrthoCameraDistance = 5000.0f;
static constexpr float OrthoMinHalfHeight = 0.05f;
static constexpr float OrthoMaxHalfHeight = 4000.0f;

EditorOrthoCameraInputHandler::EditorOrthoCameraInputHandler()
    : m_controller(nullptr)
{
}

EditorOrthoCameraInputHandler::EditorOrthoCameraInputHandler(EditorOrthoCameraController* controller)
    : m_controller(controller)
{
}

bool EditorOrthoCameraInputHandler::OnMouseDrag(const MouseEvent& evt)
{
    if (!m_controller || evt.mouseButtons == MouseButtonState::NONE)
    {
        return false;
    }

    m_controller->Pan(evt.relativePos - evt.relativePrevPos);

    return true;
}

bool EditorOrthoCameraInputHandler::OnMouseScroll(const MouseEvent& evt)
{
    if (!m_controller || evt.wheel.y == 0)
    {
        return false;
    }

    m_controller->Zoom(float(evt.wheel.y) / 120.0f, evt.relativePos);

    return true;
}

#pragma endregion EditorOrthoCameraInputHandler

#pragma region EditorOrthoCameraController

EditorOrthoCameraController::EditorOrthoCameraController()
    : EditorOrthoCameraController(EditorViewportType::Top)
{
}

EditorOrthoCameraController::EditorOrthoCameraController(EditorViewportType viewportType)
    : CameraController(CameraProjectionMode::ORTHOGRAPHIC),
      m_viewportType(viewportType),
      m_halfHeight(20.0f)
{
    m_inputHandler = MakeHandle<EditorOrthoCameraInputHandler>(this);
    InitObject(m_inputHandler);
}

Vec3f EditorOrthoCameraController::GetViewDirection(EditorViewportType viewportType)
{
    switch (viewportType)
    {
    case EditorViewportType::Top:
        return -Vec3f::UnitY();
    case EditorViewportType::Front:
        return -Vec3f::UnitZ();
    case EditorViewportType::Side:
        return -Vec3f::UnitX();
    default:
        return Vec3f::UnitZ();
    }
}

Vec3f EditorOrthoCameraController::GetViewUp(EditorViewportType viewportType)
{
    return viewportType == EditorViewportType::Top ? -Vec3f::UnitZ() : Vec3f::UnitY();
}

Vec3f EditorOrthoCameraController::GetViewSide(EditorViewportType viewportType)
{
    return GetViewUp(viewportType).Cross(GetViewDirection(viewportType));
}

void EditorOrthoCameraController::OnActivated()
{
    CameraController::OnActivated();

    const Vec3f direction = GetViewDirection(m_viewportType);

    m_camera->SetWorldRotation(Quat4f::LookAt(direction, GetViewUp(m_viewportType)).Inverse());

    Vec3f translation = m_camera->GetWorldTranslation();
    translation -= direction * translation.Dot(direction);
    translation -= direction * OrthoCameraDistance;

    m_camera->SetWorldTranslation(translation);

    m_camera->SetNearClip(1.0f);
    m_camera->SetFarClip(OrthoCameraDistance * 2.0f);
}

void EditorOrthoCameraController::SetHalfHeight(float halfHeight)
{
    m_halfHeight = MathUtil::Clamp(halfHeight, OrthoMinHalfHeight, OrthoMaxHalfHeight);

    UpdateProjectionMatrix();
}

Vec2f EditorOrthoCameraController::GetWorldSize() const
{
    const Vec2i dimensions = m_camera ? m_camera->GetDimensions() : Vec2i::One();
    const float aspect = dimensions.y > 0 ? float(dimensions.x) / float(dimensions.y) : 1.0f;

    return Vec2f(m_halfHeight * 2.0f * aspect, m_halfHeight * 2.0f);
}

void EditorOrthoCameraController::Pan(Vec2f screenDelta)
{
    if (!m_camera)
    {
        return;
    }

    const Vec2f worldSize = GetWorldSize();

    const Vec3f offset = (GetViewSide(m_viewportType) * (-screenDelta.x * worldSize.x))
        + (GetViewUp(m_viewportType) * (screenDelta.y * worldSize.y));

    m_camera->SetWorldTranslation(m_camera->GetWorldTranslation() + offset);
}

void EditorOrthoCameraController::Zoom(float wheelSteps, Vec2f screenPosition)
{
    if (!m_camera)
    {
        return;
    }

    const Vec2f worldSizeBefore = GetWorldSize();

    SetHalfHeight(m_halfHeight * MathUtil::Pow(0.85f, wheelSteps));

    const Vec2f worldSizeAfter = GetWorldSize();

    // keep the point under the cursor fixed
    const Vec2f cursorFromCenter = screenPosition - Vec2f(0.5f, 0.5f);
    const Vec2f shift = cursorFromCenter * (worldSizeBefore - worldSizeAfter);

    const Vec3f offset = (GetViewSide(m_viewportType) * shift.x) - (GetViewUp(m_viewportType) * shift.y);

    m_camera->SetWorldTranslation(m_camera->GetWorldTranslation() + offset);
}

void EditorOrthoCameraController::CenterOn(const Vec3f& point)
{
    if (!m_camera)
    {
        return;
    }

    const Vec3f direction = GetViewDirection(m_viewportType);

    m_camera->SetWorldTranslation(point - direction * (point.Dot(direction) + OrthoCameraDistance));
}

void EditorOrthoCameraController::UpdateLogic(double delta)
{
}

void EditorOrthoCameraController::UpdateViewMatrix()
{
    m_camera->m_viewMat = Mat4f::LookAt(
        m_camera->GetWorldTranslation(),
        m_camera->GetTarget(),
        GetViewUp(m_viewportType));
}

void EditorOrthoCameraController::UpdateProjectionMatrix()
{
    if (!m_camera)
    {
        return;
    }

    const Vec2f halfSize = GetWorldSize() * 0.5f;

    m_camera->SetToOrthographicProjection(
        -halfSize.x, halfSize.x,
        -halfSize.y, halfSize.y,
        m_camera->GetNearClip(), m_camera->GetFarClip());
}

#pragma endregion EditorOrthoCameraController

} // namespace Hyperion
