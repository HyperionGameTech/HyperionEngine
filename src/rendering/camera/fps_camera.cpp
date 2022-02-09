#include "fps_camera.h"

namespace hyperion {
FpsCamera::FpsCamera(InputManager *inputmgr, RenderWindow *window, int width, int height, float fov, float near, float far)
    : PerspectiveCamera(fov, width, height, near, far),
      m_inputmgr(inputmgr),
      m_window(window),
      m_mag_x(0.0),
      m_mag_y(0.0),
      m_old_mag_x(0.0),
      m_old_mag_y(0.0),
      m_mouse_x(0.0),
      m_mouse_y(0.0),
      m_is_mouse_captured(false)
{
    m_dir_cross_y = Vector3(m_direction).Cross(m_up);

    m_inputmgr->RegisterKeyEvent(KeyboardKey::KEY_LEFT_ALT, InputEvent([&](bool pressed) {
        if (pressed) {
            CenterMouse();

            m_is_mouse_captured = !m_is_mouse_captured;

            CoreEngine::GetInstance()->SetCursorLocked(m_is_mouse_captured);
        }
    }));
}

void FpsCamera::SetTranslation(const Vector3 &vec)
{
    Camera::SetTranslation(vec);
    m_next_translation = m_translation;
}

void FpsCamera::UpdateLogic(double dt)
{
    m_width = m_window->GetScaledWidth();
    m_height = m_window->GetScaledHeight();

    if (m_is_mouse_captured) {
        m_mouse_x = m_inputmgr->GetMouseX();
        m_mouse_y = m_inputmgr->GetMouseY();

        HandleMouseInput(dt, m_window->GetWidth() / 2, m_window->GetHeight() / 2);

        CenterMouse();
    }

    HandleKeyboardInput(dt);
}

void FpsCamera::CenterMouse()
{
    m_inputmgr->SetMousePosition(m_window->GetWidth() / 2, m_window->GetHeight() / 2);
}

void FpsCamera::HandleMouseInput(double dt, int half_width, int half_height)
{
    const double sensitivity = 0.1;
    const double smoothing = 10.0;

    m_mag_x = m_mouse_x - half_width;
    m_mag_y = m_mouse_y - half_height;

    m_mag_x = MathUtil::Lerp(
        m_old_mag_x,
        m_mag_x,
        MathUtil::Clamp(smoothing * dt, 0.0, 1.0)
    );

    m_mag_y = MathUtil::Lerp(
        m_old_mag_y,
        m_mag_y,
        MathUtil::Clamp(smoothing * dt, 0.0, 1.0)
    );

    m_old_mag_x = m_mag_x;
    m_old_mag_y = m_mag_y;

    m_dir_cross_y = Vector3(m_direction).Cross(m_up);

    Rotate(m_up, MathUtil::DegToRad(m_mag_x * sensitivity));
    Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag_y * sensitivity));

    if (m_direction.y > 0.97f || m_direction.y < -0.97f) {
        m_mag_y *= -1.0f;
        Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag_y * sensitivity));
    }
}

void FpsCamera::HandleKeyboardInput(double dt)
{
    double speed = dt * 5.0;

    if (m_inputmgr->IsKeyDown(KEY_LEFT_SHIFT) || m_inputmgr->IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed *= 2.0;
    }

    if (m_inputmgr->IsKeyDown(KEY_W)) {
        m_next_translation += m_direction * speed;
    } else if (m_inputmgr->IsKeyDown(KEY_S)) {
        m_next_translation -= m_direction * speed;
    }

    if (m_inputmgr->IsKeyDown(KEY_A)) {
        m_next_translation -= m_dir_cross_y * speed;
    } else if (m_inputmgr->IsKeyDown(KEY_D)) {
        m_next_translation += m_dir_cross_y * speed;
    }

    if (m_translation != m_next_translation) {
        std::cout << " Move to " << m_next_translation << "\n";
    }

    m_translation.Lerp(m_next_translation, MathUtil::Clamp(2.0 * dt, 0.0, 1.0));
}
} // namespace hyperion
