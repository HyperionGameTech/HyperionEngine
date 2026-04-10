#pragma once


/**
 * @brief Enum of all scancode values accepted by Foxtrot.
 *
 * This enum uses scancode values to provide a position independent representation of the keyboard.
 *
 * This enum is based on a modified version of the SDL_Scancode enum.
 */
enum class FxKey
{
    FX_KEY_UNKNOWN = 0,

    FX_KEYBOARD_BEGIN = 4,
    FX_KEYBOARD_STANDARD_BEGIN = 4,

    FX_KEY_A = 4,
    FX_KEY_B = 5,
    FX_KEY_C = 6,
    FX_KEY_D = 7,
    FX_KEY_E = 8,
    FX_KEY_F = 9,
    FX_KEY_G = 10,
    FX_KEY_H = 11,
    FX_KEY_I = 12,
    FX_KEY_J = 13,
    FX_KEY_K = 14,
    FX_KEY_L = 15,
    FX_KEY_M = 16,
    FX_KEY_N = 17,
    FX_KEY_O = 18,
    FX_KEY_P = 19,
    FX_KEY_Q = 20,
    FX_KEY_R = 21,
    FX_KEY_S = 22,
    FX_KEY_T = 23,
    FX_KEY_U = 24,
    FX_KEY_V = 25,
    FX_KEY_W = 26,
    FX_KEY_X = 27,
    FX_KEY_Y = 28,
    FX_KEY_Z = 29,

    FX_KEY_1 = 30,
    FX_KEY_2 = 31,
    FX_KEY_3 = 32,
    FX_KEY_4 = 33,
    FX_KEY_5 = 34,
    FX_KEY_6 = 35,
    FX_KEY_7 = 36,
    FX_KEY_8 = 37,
    FX_KEY_9 = 38,
    FX_KEY_0 = 39,

    FX_KEY_RETURN = 40,
    FX_KEY_ESCAPE = 41,
    FX_KEY_BACKSPACE = 42,
    FX_KEY_TAB = 43,
    FX_KEY_SPACE = 44,

    FX_KEY_MINUS = 45,
    FX_KEY_EQUALS = 46,
    FX_KEY_LBRACKET = 47,
    FX_KEY_RBRACKET = 48,
    FX_KEY_BACKSLASH = 49,

    FX_KEY_SEMICOLON = 51,
    FX_KEY_APOSTROPHE = 52,
    FX_KEY_GRAVE = 53,
    FX_KEY_TILDE = FX_KEY_GRAVE,
    FX_KEY_COMMA = 54,
    FX_KEY_PERIOD = 55,
    FX_KEY_SLASH = 56,

    FX_KEY_CAPSLOCK = 57,

    FX_KEY_F1 = 58,
    FX_KEY_F2 = 59,
    FX_KEY_F3 = 60,
    FX_KEY_F4 = 61,
    FX_KEY_F5 = 62,
    FX_KEY_F6 = 63,
    FX_KEY_F7 = 64,
    FX_KEY_F8 = 65,
    FX_KEY_F9 = 66,
    FX_KEY_F10 = 67,
    FX_KEY_F11 = 68,
    FX_KEY_F12 = 69,

    FX_KEY_PRINTSCREEN = 70,
    FX_KEY_SCROLLLOCK = 71,
    FX_KEY_PAUSE = 72,

    FX_KEY_INSERT = 73,   /** Insert on PC's, Help on macOS. Equivalent to FX_KEY_MAC_HELP */
    FX_KEY_MAC_HELP = 73, /** Help on macOS, Insert on PC's. Equivalent to FX_KEY_INSERT. */

    FX_KEY_HOME = 74,
    FX_KEY_PAGEUP = 75,
    FX_KEY_DELETE = 76,
    FX_KEY_END = 77,
    FX_KEY_PAGEDOWN = 78,
    FX_KEY_RIGHT = 79,
    FX_KEY_LEFT = 80,
    FX_KEY_DOWN = 81,
    FX_KEY_UP = 82,

    FX_KEY_NUMLOCK = 83,   /** Numlock on PC's, Clear on macOS. Equivalent to FX_KEY_MAC_CLEAR */
    FX_KEY_MAC_CLEAR = 83, /** Clear on macOS, Numlock on PC's. Equivalent to FX_KEY_NUMLOCK */

    FX_KEY_NUMPAD_DIVIDE = 84,
    FX_KEY_NUMPAD_MULTIPLY = 85,
    FX_KEY_NUMPAD_MINUS = 86,
    FX_KEY_NUMPAD_PLUS = 87,
    FX_KEY_NUMPAD_ENTER = 88,
    FX_KEY_NUMPAD_1 = 89,
    FX_KEY_NUMPAD_2 = 90,
    FX_KEY_NUMPAD_3 = 91,
    FX_KEY_NUMPAD_4 = 92,
    FX_KEY_NUMPAD_5 = 93,
    FX_KEY_NUMPAD_6 = 94,
    FX_KEY_NUMPAD_7 = 95,
    FX_KEY_NUMPAD_8 = 96,
    FX_KEY_NUMPAD_9 = 97,
    FX_KEY_NUMPAD_0 = 98,
    FX_KEY_NUMPAD_PERIOD = 99,
    /* End of Standard Keys */

    FX_KEYBOARD_STANDARD_END = 99,

    /* Special Keys */
    FX_KEYBOARD_MODIFIERS_START = 100, /* Offset by -FX_KEYBOARD_MODIFIERS_OFFSET */

    /* Left modifier keys */
    FX_KEY_LCTRL = 100,
    FX_KEY_LSHIFT = 101,
    FX_KEY_LALT = 102,
    FX_KEY_LMETA = 103,
    /* Right modifier keys */
    FX_KEY_RCTRL = 104,
    FX_KEY_RSHIFT = 105,
    FX_KEY_RALT = 106,
    FX_KEY_RMETA = 107,

    FX_KEYBOARD_MODIFIERS_END = 107,

    FX_KEYBOARD_END = 107,

    /* Mouse buttons */
    FX_MOUSE_BUTTONS_START = 108,

    FX_MOUSE_UNUSED = 108,
    FX_MOUSE_LEFT = 109,
    FX_MOUSE_MIDDLE = 110,
    FX_MOUSE_RIGHT = 111,

    FX_MOUSE_BUTTONS_END,

    FX_KEY_MAX
};

/// The offset to get from the underlying key (SDL_SCANCODE_LCTRL, etc.) to our keycode. This only applies to
/// The modifier keys.
/// To convert to the underlying keycode, add this value, and vice versa to get our keycode.
#define FX_KEYBOARD_MODIFIERS_OFFSET (124)

#define FX_MOUSE_BUTTON_OFFSET (-FX_KEYBOARD_END)
