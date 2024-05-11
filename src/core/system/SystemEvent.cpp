#include <core/system/SystemEvent.hpp>
#include <core/containers/Bitset.hpp>

namespace hyperion {
namespace sys {

#pragma region Helper methods

#ifdef HYP_SDL

static EnumFlags<MouseButtonState> GetMouseButtonState(int sdl_button)
{
    EnumFlags<MouseButtonState> mouse_button_state = MouseButtonState::NONE;

    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        mouse_button_state |= MouseButtonState::LEFT;
        break;
    case SDL_BUTTON_MIDDLE:
        mouse_button_state |= MouseButtonState::MIDDLE;
        break;
    case SDL_BUTTON_RIGHT:
        mouse_button_state |= MouseButtonState::RIGHT;
        break;
    default:
        break;
    }

    // Bitset bitset { uint32(sdl_button) };

    // Bitset::BitIndex first_set_bit_index = -1;

    // while ((first_set_bit_index = bitset.FirstSetBitIndex()) != -1) {
    //     switch (first_set_bit_index + 1) {
    //     case SDL_BUTTON_LEFT:
    //         mouse_button_state |= MouseButtonState::LEFT;
    //         break;
    //     case SDL_BUTTON_MIDDLE:
    //         mouse_button_state |= MouseButtonState::MIDDLE;
    //         break;
    //     case SDL_BUTTON_RIGHT:
    //         mouse_button_state |= MouseButtonState::RIGHT;
    //         break;
    //     default:
    //         break;
    //     }

    //     bitset.Set(first_set_bit_index, false);
    // }

    return mouse_button_state;
}

#endif

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

EnumFlags<MouseButtonState> SystemEvent::GetMouseButtons() const
{
    return GetMouseButtonState(sdl_event.button.button);
}

#pragma endregion SystemEvent

} // namespace sys
} // namespace hyperion