#include <core/system/SystemEvent.hpp>

namespace hyperion {
namespace sys {

#pragma region Helper methods

static MouseButton GetMouseButtonFromSDLMouseButton(int sdl_button)
{
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::LEFT;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::MIDDLE;
    case SDL_BUTTON_RIGHT:
        return MouseButton::RIGHT;
    default:
        return MouseButton::INVALID;
    }
}

#pragma endregion Helper methods

#pragma region SystemEvent

SDL_Event *SystemEvent::GetInternalEvent()
{
    return &(this->sdl_event);
}

KeyCode SystemEvent::GetKeyCode() const
{
    return KeyCode(sdl_event.key.keysym.sym);
}

MouseButton SystemEvent::GetMouseButton() const
{
    return GetMouseButtonFromSDLMouseButton(sdl_event.button.button);
}

#pragma endregion SystemEvent

} // namespace sys
} // namespace hyperion