/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Camera/ThirdPersonCamera.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <Framework/Game.hpp>

#include <Scene/World.hpp>
#include <Scene/Input/TouchControlsSubsystem.hpp>

#include <ThirdPersonCamera.generated.inl>

namespace Hyperion {

static constexpr float TouchLookSensitivity = 80.0f;
static constexpr float ZoomSharpness = 12.0f;
static constexpr float ControllerLookSharpness = 18.0f;
static constexpr float ControllerTurnBoostThreshold = 0.9f;
static constexpr float ControllerTurnBoostDelay = 0.2f;
static constexpr float ControllerTurnBoostRampTime = 0.4f;

#pragma region ThirdPersonCameraInputHandler

ThirdPersonCameraInputHandler::ThirdPersonCameraInputHandler()
    : m_controller(nullptr)
{
}

ThirdPersonCameraInputHandler::ThirdPersonCameraInputHandler(ThirdPersonCameraController* controller)
    : m_controller(controller)
{
    Assert(m_controller != nullptr);
}

bool ThirdPersonCameraInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    if (evt.keyCode == KeyCode::KEY_ESCAPE)
    {
        m_controller->SetMouseLocked(false);
    }

    return InputHandlerBase::OnKeyDown(evt);
}

bool ThirdPersonCameraInputHandler::OnMouseDown(const MouseEvent& evt)
{
    m_controller->SetMouseLocked(true);

    return InputHandlerBase::OnMouseDown(evt);
}

bool ThirdPersonCameraInputHandler::OnMouseUp(const MouseEvent& evt)
{
#if HYP_ANDROID || HYP_IOS
    m_controller->SetMouseLocked(false);
#endif

    return InputHandlerBase::OnMouseUp(evt);
}

bool ThirdPersonCameraInputHandler::OnMouseMove(const MouseEvent& evt)
{
    HYP_SCOPE;

    if (!m_controller || !m_controller->IsMouseLockRequested())
    {
        return false;
    }

    const Vec2f mouseDelta = (evt.relativePos - evt.relativePrevPos) * m_controller->GetMouseSensitivity();

    m_controller->AddYawPitch(mouseDelta.x, mouseDelta.y);

    return true;
}

bool ThirdPersonCameraInputHandler::OnMouseScroll(const MouseEvent& evt)
{
    if (!m_controller || evt.wheel.y == 0)
    {
        return false;
    }

    m_controller->Zoom(-float(evt.wheel.y));

    return true;
}

bool ThirdPersonCameraInputHandler::OnTouchMove(const TouchEvent& evt)
{
    HYP_SCOPE;

    if (!m_controller || !ShouldProcessTouch(evt))
    {
        return false;
    }

    const Vec2f touchDelta = evt.relativeDelta * TouchLookSensitivity;

    m_controller->AddYawPitch(touchDelta.x, touchDelta.y);

    return true;
}

bool ThirdPersonCameraInputHandler::ShouldProcessTouch(const TouchEvent& evt) const
{
    if (!g_gameInstance)
    {
        return false;
    }

    World* world = g_gameInstance->GetWorld().Get();

    if (!world)
    {
        return false;
    }

    TouchControlsSubsystem* touchControls = world->GetSubsystem<TouchControlsSubsystem>();

    if (!touchControls)
    {
        return false;
    }

    TouchPoint touchPoint;

    if (!touchControls->GetTouchPoint(evt.pointerId, touchPoint) || touchPoint.isLeftSide)
    {
        return false;
    }

    return true;
}

bool ThirdPersonCameraInputHandler::OnGainFocus(const MouseEvent& evt)
{
    InputHandlerBase::OnGainFocus(evt);

    m_controller->SetMouseLocked(true);

    return true;
}

bool ThirdPersonCameraInputHandler::OnLoseFocus(const MouseEvent& evt)
{
    InputHandlerBase::OnLoseFocus(evt);

    m_controller->SetMouseLocked(false);

    return true;
}

bool ThirdPersonCameraInputHandler::OnControllerButtonDown(ControllerButton btn)
{
    switch (btn)
    {
    case ControllerButton::Guide:
        m_controller->SetMouseLocked(false);
        break;
    case ControllerButton::A:
        m_controller->SetMouseLocked(true);
        break;
    default:
        break;
    }

    return false;
}

#pragma endregion ThirdPersonCameraInputHandler

#pragma region ThirdPersonCameraController

ThirdPersonCameraController::ThirdPersonCameraController()
    : PerspectiveCameraController()
{
    m_inputHandler = MakeHandle<ThirdPersonCameraInputHandler>(this);
}

void ThirdPersonCameraController::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    CameraController::Init();

    InitObject(m_inputHandler);
}

void ThirdPersonCameraController::OnActivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnActivated();

    SetMouseLocked(false);

    m_hasSmoothedPivot = false;
    m_currentDistance = MathUtil::Clamp(m_distance, m_minDistance, m_maxDistance);

    m_controllerLookRate = Vec2f::Zero();
    m_controllerTurnHoldTime = 0.0f;

    DetachFromParentTransform();
}

void ThirdPersonCameraController::OnDeactivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnDeactivated();

    RestoreParentTransform();
}

void ThirdPersonCameraController::OnRemoved()
{
    RestoreParentTransform();

    PerspectiveCameraController::OnRemoved();
}

void ThirdPersonCameraController::DetachFromParentTransform()
{
    if (!m_camera)
    {
        return;
    }

    const EnumFlags<NodeFlags> nodeFlags = m_camera->GetNodeFlags();

    if ((nodeFlags & NodeFlags::IgnoreParentTransform) == NodeFlags::IgnoreParentTransform)
    {
        return;
    }

    m_camera->SetNodeFlags(nodeFlags | NodeFlags::IgnoreParentTransform);
    m_addedIgnoreParentTransform = true;
}

void ThirdPersonCameraController::RestoreParentTransform()
{
    if (!m_camera || !m_addedIgnoreParentTransform)
    {
        return;
    }

    m_camera->SetNodeFlags(m_camera->GetNodeFlags() & ~NodeFlags::IgnoreParentTransform);
    m_addedIgnoreParentTransform = false;
}

void ThirdPersonCameraController::SetMouseLocked(bool mouseLocked)
{
    CameraController::SetIsMouseLockRequested(mouseLocked);
}

void ThirdPersonCameraController::SetDistance(float distance)
{
    m_distance = MathUtil::Clamp(distance, m_minDistance, m_maxDistance);
}

void ThirdPersonCameraController::AddYawPitch(float yawDegrees, float pitchDegrees)
{
    m_yaw = std::fmod(m_yaw + yawDegrees, 360.0f);
    m_pitch = MathUtil::Clamp(m_pitch + pitchDegrees, m_minPitch, m_maxPitch);
}

void ThirdPersonCameraController::Zoom(float amount)
{
    SetDistance(m_distance + amount * m_zoomStep);
}

Vec3f ThirdPersonCameraController::GetViewDirection() const
{
    const float yawRadians = MathUtil::DegToRad(m_yaw);
    const float pitchRadians = MathUtil::DegToRad(MathUtil::Clamp(m_pitch, m_minPitch, m_maxPitch));

    return Vec3f(
        MathUtil::Sin(yawRadians) * MathUtil::Cos(pitchRadians),
        -MathUtil::Sin(pitchRadians),
        MathUtil::Cos(yawRadians) * MathUtil::Cos(pitchRadians));
}

Vec3f ThirdPersonCameraController::CalculatePivotTarget() const
{
    const Node* parentNode = m_camera->GetParent();

    const Vec3f origin = parentNode != nullptr
        ? parentNode->GetWorldTranslation()
        : Vec3f::Zero();

    return origin + m_pivotOffset;
}

Vec2f ThirdPersonCameraController::UpdateControllerLookRate(const Vec2f& stick, float deltaSeconds)
{
    const float stickLength = stick.Length();
    const float deadzone = MathUtil::Clamp(m_controllerDeadzone, 0.0f, 0.95f);

    Vec2f targetRate;

    if (stickLength > deadzone)
    {
        const Vec2f stickDirection = stick / stickLength;
        const float rescaledMagnitude = MathUtil::Min((stickLength - deadzone) / (1.0f - deadzone), 1.0f);
        const float responseMagnitude = MathUtil::Pow(rescaledMagnitude, MathUtil::Max(m_controllerResponseExponent, 0.1f));

        const Vec2f response = stickDirection * responseMagnitude;

        // Holding the stick fully sideways ramps in extra yaw so large turns don't need a lower base sensitivity
        if (MathUtil::Abs(response.x) >= ControllerTurnBoostThreshold)
        {
            m_controllerTurnHoldTime += deltaSeconds;
        }
        else
        {
            m_controllerTurnHoldTime = 0.0f;
        }

        const float boostFraction = MathUtil::Clamp((m_controllerTurnHoldTime - ControllerTurnBoostDelay) / ControllerTurnBoostRampTime, 0.0f, 1.0f);
        const float yawSpeed = m_controllerYawSpeed * (1.0f + m_controllerTurnBoost * boostFraction);

        const float pitchSign = m_invertControllerPitch ? -1.0f : 1.0f;

        targetRate = Vec2f(response.x * yawSpeed, response.y * m_controllerPitchSpeed * pitchSign);
    }
    else
    {
        m_controllerTurnHoldTime = 0.0f;
    }

    const float lookAlpha = MathUtil::Clamp(1.0f - MathUtil::Exp(-ControllerLookSharpness * deltaSeconds), 0.0f, 1.0f);

    m_controllerLookRate = m_controllerLookRate + (targetRate - m_controllerLookRate) * lookAlpha;

    if (targetRate.IsZero() && m_controllerLookRate.LengthSquared() < 0.01f)
    {
        m_controllerLookRate = Vec2f::Zero();
    }

    return m_controllerLookRate;
}

void ThirdPersonCameraController::UpdateLogic(double delta)
{
    HYP_SCOPE;

    const float deltaSeconds = float(delta);

    m_inputHandler->SetDeltaTime(delta);

    const Vec2f controllerLookRate = UpdateControllerLookRate(m_inputHandler->GetControllerLookDelta(), deltaSeconds);

    if (!controllerLookRate.IsZero())
    {
        AddYawPitch(controllerLookRate.x * deltaSeconds, controllerLookRate.y * deltaSeconds);
    }

    DetachFromParentTransform();

    const Vec3f pivotTarget = CalculatePivotTarget();

    if (!m_hasSmoothedPivot || m_followSharpness <= 0.0f)
    {
        m_smoothedPivot = pivotTarget;
        m_hasSmoothedPivot = true;
    }
    else
    {
        const float followAlpha = 1.0f - MathUtil::Exp(-m_followSharpness * deltaSeconds);

        m_smoothedPivot.Lerp(pivotTarget, MathUtil::Clamp(followAlpha, 0.0f, 1.0f));
    }

    const float zoomAlpha = 1.0f - MathUtil::Exp(-ZoomSharpness * deltaSeconds);
    m_currentDistance = MathUtil::Lerp(m_currentDistance, MathUtil::Clamp(m_distance, m_minDistance, m_maxDistance), MathUtil::Clamp(zoomAlpha, 0.0f, 1.0f));

    const Vec3f viewDirection = GetViewDirection();

    Vec3f flatForward = Vec3f(viewDirection.x, 0.0f, viewDirection.z);
    flatForward = flatForward.LengthSquared() > 0.0001f ? flatForward.Normalized() : Vec3f::UnitZ();

    const Vec3f right = Vec3f::UnitY().Cross(flatForward).Normalized();

    const Vec3f pivot = m_smoothedPivot + right * m_shoulderOffset;

    float eyeDistance = m_currentDistance;

    // Looking up swings the eye below the pivot; pull in rather than sinking through the ground
    if (viewDirection.y > 0.0001f && m_minEyeHeight < 0.0f)
    {
        eyeDistance = MathUtil::Min(eyeDistance, -m_minEyeHeight / viewDirection.y);
    }

    const Vec3f eyePosition = pivot - viewDirection * eyeDistance;

    m_camera->SetWorldTranslation(eyePosition, TransformChangeType::Simulation);
    m_camera->SetDirection(viewDirection);
}

#pragma endregion ThirdPersonCameraController

} // namespace Hyperion
