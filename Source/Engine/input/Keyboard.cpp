/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <input/Keyboard.hpp>
#include <input/InputManager.hpp>
#include <input/Event.hpp>

#include <Core/debug/Debug.hpp>

#include <Keyboard.generated.inl>

namespace Hyperion {

HYP_API bool KeyCodeToChar(KeyCode keyCode, bool shift, bool alt, bool ctrl, char& outChar)
{
    if (uint32(keyCode) >= uint32(KeyCode::KEY_A) && uint32(keyCode) <= uint32(KeyCode::KEY_Z))
    {
        outChar = char(uint32(keyCode) - uint32(KeyCode::KEY_A)) + (shift ? 'A' : 'a');
        return true;
    }

    if (uint32(keyCode) >= uint32(KeyCode::KEY_0) && uint32(keyCode) <= uint32(KeyCode::KEY_9))
    {
        static constexpr char NumericKeyCodes[] = {
            ')', '!', '@', '#', '$', '%', '^', '&', '*', '('
        };

        outChar = (shift ? NumericKeyCodes[uint32(keyCode) - uint32(KeyCode::KEY_0)] : (char(uint32(keyCode) - uint32(KeyCode::KEY_0)) + '0'));
        return true;
    }

    switch ((int)keyCode)
    {
    case (int)' ':
        outChar = ' ';
        return true;
    case (int)'`':
        outChar = shift ? '~' : '`';
        return true;
    case (int)',':
        outChar = shift ? '<' : ',';
        return true;
    case (int)'.':
        outChar = shift ? '>' : '.';
        return true;
    case (int)'/':
        outChar = shift ? '?' : '/';
        return true;
    case (int)';':
        outChar = shift ? ':' : ';';
        return true;
    case (int)'\'':
        outChar = shift ? '"' : '\'';
        return true;
    case (int)'-':
        outChar = shift ? '_' : '-';
        return true;
    case (int)'=':
        outChar = shift ? '+' : '=';
        return true;
    case (int)'\r':
        outChar = '\n';
        return true;
    case (int)'\n':
        outChar = '\n';
        return true;
    case (int)'\t':
        outChar = '\t';
        return true;
    case (int)'\b':
        outChar = '\b';
        return true;

    default:
        break;
    }

    return false;
}

} // namespace Hyperion
