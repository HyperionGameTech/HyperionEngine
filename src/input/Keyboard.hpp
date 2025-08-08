#pragma once
#include <core/Defines.hpp>

#include <core/Types.hpp>

namespace hyperion {

class InputManager;

HYP_ENUM()
enum class KeyCode : uint16
{
    UNKNOWN = uint16(-1),

    KEY_A = 97,
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

    KEY_0 = 48,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_F1 = 58,
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

    LEFT_CTRL = 224,
    LEFT_SHIFT = 225,
    LEFT_ALT = 226,
    RIGHT_CTRL = 228,
    RIGHT_SHIFT = 229,
    RIGHT_ALT = 230,

    SPACE = 32,
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

HYP_API bool KeyCodeToChar(KeyCode keyCode, bool shift, bool alt, bool ctrl, char& outChar);

HYP_STRUCT(Size = 16)

struct KeyboardEvent
{
    HYP_FIELD()
    InputManager* inputManager = nullptr;

    HYP_FIELD()
    KeyCode keyCode = KeyCode::UNKNOWN;
};

} // namespace hyperion
