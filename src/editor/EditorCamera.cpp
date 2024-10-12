/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

namespace hyperion {

#pragma region EditorCameraInputHandler

class EditorCameraInputHandler : public InputHandler
{
public:
    EditorCameraInputHandler(CameraController *controller)
        : m_controller(dynamic_cast<EditorCameraController *>(controller))
    {
        AssertThrowMsg(m_controller != nullptr, "Null camera controller or not of type EditorCameraController");
    }

    virtual bool OnKeyDown(const KeyboardEvent &event) override
    {
        Camera *camera = m_controller->GetCamera();

        if (event.key_code == KeyCode::KEY_W || event.key_code == KeyCode::KEY_S || event.key_code == KeyCode::KEY_A || event.key_code == KeyCode::KEY_D) {
            CameraController *camera_controller = camera->GetCameraController();

            Vec3f translation = camera->GetTranslation();

            const Vec3f direction = camera->GetDirection();
            const Vec3f dir_cross_y = direction.Cross(camera->GetUpVector());

            if (event.key_code == KeyCode::KEY_W) {
                translation += direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_S) {
                translation -= direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_A) {
                translation += dir_cross_y * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_D) {
                translation -= dir_cross_y * 0.01f;
            }

            camera_controller->SetNextTranslation(translation);

            return true;
        }

        return false;
    }

    virtual bool OnKeyUp(const KeyboardEvent &event) override
    {
        return false;
    }

    virtual bool OnMouseDown(const MouseEvent &event) override
    {
        m_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);

        return true;
    }

    virtual bool OnMouseUp(const MouseEvent &event) override
    {
        m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

        return true;
    }

    virtual bool OnMouseMove(const MouseEvent &event) override
    {
        return false;
    }

    virtual bool OnMouseDrag(const MouseEvent &event) override
    {
        Camera *camera = m_controller->GetCamera();
        
        if (!camera) {
            return false;
        }

        const Vec2f delta = (event.position - event.previous_position) * 150.0f;

        const Vec3f dir_cross_y = camera->GetDirection().Cross(camera->GetUpVector());

        if (event.mouse_buttons & MouseButtonState::LEFT) {
            const Vec2i delta_sign = Vec2i(MathUtil::Sign(delta));

            if (event.mouse_buttons & MouseButtonState::RIGHT) {
                camera->SetNextTranslation(camera->GetTranslation() + dir_cross_y * 0.1f * float(-delta_sign.y));
            } else {
                camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));

                camera->SetNextTranslation(camera->GetTranslation() + camera->GetDirection() * 0.1f * float(-delta_sign.y));
            }
        } else if (event.mouse_buttons & MouseButtonState::RIGHT) {
            camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
            camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

            if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f) {
                camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
            }
        }

        return true;
    }

    virtual bool OnClick(const MouseEvent &event) override
    {
        return false;
    }

private:
    EditorCameraController  *m_controller;
};

#pragma endregion EditorCameraInputHandler

#pragma region EditorCameraController

EditorCameraController::EditorCameraController()
    : FirstPersonCameraController(),
      m_mode(EditorCameraControllerMode::INACTIVE)
{
    m_input_handler = MakeUnique<EditorCameraInputHandler>(this);
}

void EditorCameraController::SetMode(EditorCameraControllerMode mode)
{
    m_mode = mode;

    switch (mode) {
    case EditorCameraControllerMode::INACTIVE:
    case EditorCameraControllerMode::FOCUSED: // fallthrough
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);
        break;
    case EditorCameraControllerMode::MOUSE_LOCKED:
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

        break;
    }
}

void EditorCameraController::UpdateLogic(double dt)
{
    FirstPersonCameraController::UpdateLogic(dt);
}

void EditorCameraController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    case CameraCommand::CAMERA_COMMAND_MOVEMENT: // fallthrough
        if (m_mode == EditorCameraControllerMode::INACTIVE) {
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
