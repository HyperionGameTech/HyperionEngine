#pragma once

#include <core/reflection/ObjectFwd.hpp>

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {

class InputManager;

HYP_ENUM()
enum class KeyCode : uint16
{
    KEY_UNKNOWN = UINT16_MAX,

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

    KEY_LCTRL = 224,
    KEY_LSHIFT = 225,
    KEY_LALT = 226,
    KEY_RCTRL = 228,
    KEY_RSHIFT = 229,
    KEY_RALT = 230,

    KEY_SPACE = 32,
    KEY_COMMA = 44,
    KEY_DASH = 45,
    KEY_PERIOD = 46,
    KEY_RETURN = 13,
    KEY_TAB = 258,
    KEY_BACKSPACE = 8,
    KEY_CAPSLOCK = 280,
    KEY_TILDE = 96,

    KEY_RIGHT = 79,
    KEY_LEFT = 80,
    KEY_DOWN = 81,
    KEY_UP = 82,

    KEY_ESCAPE = 27
};

HYP_API bool KeyCodeToChar(KeyCode keyCode, bool shift, bool alt, bool ctrl, char& outChar);

HYP_STRUCT(Size = 16)
struct KeyboardEvent
{
    HYP_STRUCT_BODY(KeyboardEvent);

    HYP_FIELD()
    InputManager* inputManager = nullptr;

    HYP_FIELD()
    KeyCode keyCode = KeyCode::KEY_UNKNOWN;
};

} // namespace hyperion
