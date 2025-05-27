#ifndef HYPERION_KEYBOARD_HPP
#define HYPERION_KEYBOARD_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

class InputManager;

enum class KeyCode : uint16
{
    UNKNOWN = uint16(-1),

    KEY_A = 'a',
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_0 = '0',
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_F1 = 58, // SDL_SCANCODE_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    LEFT_CTRL = 224,   // SDL_SCANCODE_LCTRL,
    LEFT_SHIFT = 225,  // SDL_SCANCODE_LSHIFT,
    LEFT_ALT = 226,    // SDL_SCANCODE_LALT
    RIGHT_CTRL = 228,  // SDL_SCANCODE_RCTRL,
    RIGHT_SHIFT = 229, // SDL_SCANCODE_RSHIFT,
    RIGHT_ALT = 230,   // SDL_SCANCODE_RALT

    SPACE = 32, // SDL_SCANCODE_SPACE,
    COMMA = 44,
    DASH = 45,
    PERIOD = 46,
    RETURN = 13,
    TAB = 258,
    BACKSPACE = 8,
    CAPSLOCK = 280,
    TILDE = 96,

    ARROW_RIGHT = 79,
    ARROW_LEFT = 80,
    ARROW_DOWN = 81,
    ARROW_UP = 82,

    ESC = 27
};

HYP_API bool KeyCodeToChar(KeyCode key_code, bool shift, bool alt, bool ctrl, char& out_char);

HYP_STRUCT(Size = 16)

struct KeyboardEvent
{
    HYP_FIELD()
    InputManager* input_manager = nullptr;

    HYP_FIELD()
    KeyCode key_code = KeyCode::UNKNOWN;
};

} // namespace hyperion

#endif