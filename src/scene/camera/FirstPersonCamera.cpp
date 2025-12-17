/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <FirstPersonCamera.generated.inl>

namespace hyperion {

static const float mouseSensitivity = 1.0f;
static const float mouseBlending = 0.35f;
static const float movementSpeed = 5.0f;
static const float movementSpeed2 = movementSpeed * 2.0f;
static const float movementBlending = 0.01f;

#pragma region FirstPersonCameraInputHandler

FirstPersonCameraInputHandler::FirstPersonCameraInputHandler()
    : m_controller()
{
}

FirstPersonCameraInputHandler::FirstPersonCameraInputHandler(const WeakHandle<CameraController>& controller)
    : m_controller(WeakHandle<FirstPersonCameraController>(controller))
{
    Assert(m_controller.IsValid(), "Null camera controller or not of type FirstPersonCameraInputHandler");
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

    Handle<FirstPersonCameraController> controller = m_controller.Lock();

    if (!controller.IsValid())
    {
        return false;
    }

    Camera* camera = controller->GetCamera();

    if (!camera)
    {
        return false;
    }

    const Vec2f delta = (evt.position - evt.previousPosition) * 1.0f;

    const Vec3f dirCrossY = camera->GetDirection().Cross(camera->GetUpVector());

    camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
    camera->Rotate(dirCrossY, MathUtil::DegToRad(delta.y));

    if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f)
    {
        camera->Rotate(dirCrossY, MathUtil::DegToRad(-delta.y));
    }

    return true;
}

bool FirstPersonCameraInputHandler::OnMouseDrag_Impl(const MouseEvent& evt)
{
    HYP_SCOPE;

    return false;
}

bool FirstPersonCameraInputHandler::OnMouseLeave_Impl(const MouseEvent& evt)
{
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
      m_mouseX(0.0f),
      m_mouseY(0.0f),
      m_prevMouseX(0.0f),
      m_prevMouseY(0.0f)
{
    m_inputHandler = CreateObject<FirstPersonCameraInputHandler>(WeakHandleFromThis());
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

void FirstPersonCameraController::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    CameraController::Init();

    InitObject(m_inputHandler);
}

void FirstPersonCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;
}

#pragma endregion FirstPersonCameraController

} // namespace hyperion
