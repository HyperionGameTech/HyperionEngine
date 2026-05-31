/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <Framework/Game.hpp>

#include <FirstPersonCamera.generated.inl>

namespace Hyperion {

static constexpr float mouseSensitivity = 7000.0f;
static constexpr float mouseBlending = 0.35f;
static constexpr float movementSpeed = 5.0f;
static constexpr float movementSpeed2 = movementSpeed * 2.0f;
static constexpr float movementBlending = 0.01f;

#pragma region FirstPersonCameraInputHandler

FirstPersonCameraInputHandler::FirstPersonCameraInputHandler()
    : m_controller(nullptr)
{
}

FirstPersonCameraInputHandler::FirstPersonCameraInputHandler(FirstPersonCameraController* controller)
    : m_controller(controller)
{
    Assert(m_controller != nullptr);
}

bool FirstPersonCameraInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    if (evt.keyCode == KeyCode::KEY_ESCAPE)
    {
        m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);
    }

    return InputHandlerBase::OnKeyDown(evt);
}

bool FirstPersonCameraInputHandler::OnKeyUp(const KeyboardEvent& evt)
{
    return InputHandlerBase::OnKeyUp(evt);
}

bool FirstPersonCameraInputHandler::OnMouseDown(const MouseEvent& evt)
{
    m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

    return InputHandlerBase::OnMouseDown(evt);
}

bool FirstPersonCameraInputHandler::OnMouseUp(const MouseEvent& evt)
{
#if HYP_ANDROID || HYP_IOS // @TODO Better check for touch input. Touch devices can still have mouse input, and some non-touch devices might not have mouse input
    m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);
#endif

    return InputHandlerBase::OnMouseUp(evt);
}

bool FirstPersonCameraInputHandler::OnMouseMove(const MouseEvent& evt)
{
    HYP_SCOPE;

    if (!m_controller || !m_controller->IsMouseLockRequested())
    {
        return false;
    }

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    const float deltaTime = g_gameInstance->GetGameState().deltaTime;

    Vec2f mouseDelta = (evt.relativePos - evt.relativePrevPos) * mouseSensitivity;
    mouseDelta *= deltaTime;

    const Vec3f dirCrossY = camera->GetSideVector();

    camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(-mouseDelta.x));
    camera->Rotate(dirCrossY, MathUtil::DegToRad(-mouseDelta.y));

    if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
    {
        camera->Rotate(dirCrossY, MathUtil::DegToRad(mouseDelta.y));
    }

    return true;
}

bool FirstPersonCameraInputHandler::OnTouchMove(const TouchEvent& evt)
{
    HYP_SCOPE;

    if (!m_controller)
    {
        return false;
    }

    Camera* camera = m_controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    static constexpr float touchSensitivity = 200.0f;
    Vec2f touchDelta = evt.relativeDelta * touchSensitivity;

    const Vec3f dirCrossY = camera->GetSideVector();

    camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(-touchDelta.x));
    camera->Rotate(dirCrossY, MathUtil::DegToRad(-touchDelta.y));

    if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
    {
        camera->Rotate(dirCrossY, MathUtil::DegToRad(touchDelta.y));
    }

    return true;
}

bool FirstPersonCameraInputHandler::OnMouseDrag(const MouseEvent& evt)
{
    return false;
}

bool FirstPersonCameraInputHandler::OnMouseLeave(const MouseEvent& evt)
{
    InputHandlerBase::OnMouseLeave(evt);

    return false;
}

bool FirstPersonCameraInputHandler::OnClick(const MouseEvent& evt)
{
    return false;
}

bool FirstPersonCameraInputHandler::OnGainFocus(const MouseEvent& evt)
{
    m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);

    return true;
}

bool FirstPersonCameraInputHandler::OnLoseFocus(const MouseEvent& evt)
{
    m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

    return true;
}

#pragma endregion FirstPersonCameraInputHandler

#pragma region FirstPersonCameraController

FirstPersonCameraController::FirstPersonCameraController(FirstPersonCameraControllerMode mode)
    : PerspectiveCameraController(),
      m_mode(mode),
      m_mouseX(0.0f),
      m_mouseY(0.0f),
      m_prevMouseX(0.0f),
      m_prevMouseY(0.0f)
{
    m_inputHandler = MakeHandle<FirstPersonCameraInputHandler>(this);
}

void FirstPersonCameraController::OnActivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnActivated();

    SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);
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
    default:
        break;
    }

    m_mode = mode;
}

void FirstPersonCameraController::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    CameraController::Init();

    InitObject(m_inputHandler);
}

void FirstPersonCameraController::UpdateLogic(double delta)
{
    HYP_SCOPE;

    static constexpr float MovementSpeed = 15.0f;

    Vec3f translation = m_camera->GetWorldTranslation();

    const Vec3f direction = m_camera->GetDirection();
    const Vec3f dirCrossY = m_camera->GetSideVector();

    m_inputHandler->SetDeltaTime(delta);

    // Keyboard movement (WASD)
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_W))
    {
        translation += direction * delta * MovementSpeed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_S))
    {
        translation -= direction * delta * MovementSpeed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_A))
    {
        translation -= dirCrossY * delta * MovementSpeed;
    }
    if (m_inputHandler->IsKeyDown(KeyCode::KEY_D))
    {
        translation += dirCrossY * delta * MovementSpeed;
    }

    // Touch joystick movement
    Vec2f touchDelta = m_inputHandler->GetTouchMovementDelta();
    if (!touchDelta.IsZero())
    {
        // touchDelta.x = strafe (left/right), touchDelta.y = forward/back
        translation += direction * -touchDelta.y * delta * MovementSpeed;
        translation += dirCrossY * touchDelta.x * delta * MovementSpeed;
    }

    m_camera->SetNextTranslation(translation);
}

#pragma endregion FirstPersonCameraController

} // namespace Hyperion
