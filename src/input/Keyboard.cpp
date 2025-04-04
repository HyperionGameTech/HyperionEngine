/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/Keyboard.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

HYP_API bool KeyCodeToChar(KeyCode key_code, bool shift, bool alt, bool ctrl, char &out_char)
{
    DebugLog(LogType::Debug, "KeyCodeToChar: key_code = %u\n", uint32(key_code));

    if (uint32(key_code) >= uint32(KeyCode::KEY_A) && uint32(key_code) <= uint32(KeyCode::KEY_Z)) {
        out_char = char(uint32(key_code) - uint32(KeyCode::KEY_A)) + (shift ? 'A' : 'a');
        return true;
    }

    if (uint32(key_code) >= uint32(KeyCode::KEY_0) && uint32(key_code) <= uint32(KeyCode::KEY_9)) {
        static const char num_codes[] = {
            ')', '!', '@', '#', '$', '%', '^', '&', '*', '('
        };

        out_char = (shift ? num_codes[uint32(key_code) - uint32(KeyCode::KEY_0)] : (char(uint32(key_code) - uint32(KeyCode::KEY_0)) + '0'));
        return true;
    }

    switch (key_code) {
    case KeyCode::SPACE:
        out_char = ' ';
        return true;

    case KeyCode::PERIOD:
        out_char = '.';
        return true;

    case KeyCode::RETURN:
        out_char = '\n';
        return true;

    case KeyCode::TAB:
        out_char = '\t';
        return true;

    case KeyCode::BACKSPACE:
        out_char = '\b';
        return true;

    default:
        break;
    }

    return false;
}

} // namespace hyperion
