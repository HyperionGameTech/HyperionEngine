#include <SystemPch.hpp>

#include <mach-o/dyld.h>

#include <Core/Containers/String.hpp>

#include <Input/Keyboard.hpp>

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    char buffer[PATH_MAX];
    uint32_t bufferSize = PATH_MAX;

    if (_NSGetExecutablePath(buffer, &bufferSize) != 0)
    {
        // Buffer too small, allocate dynamically
        Array<char> dynBuffer;
        dynBuffer.Resize(bufferSize);

        if (_NSGetExecutablePath(dynBuffer.Data(), &bufferSize) != 0)
        {
            return PlatformString();
        }

        return PlatformString(dynBuffer.Data(), dynBuffer.Data() + bufferSize - 1);
    }

    // Get the actual length of the path
    size_t pathLength = std::strlen(buffer);
    return PlatformString(buffer, buffer + pathLength);
}

ENGINE_API bool IsOnBatteryPower()
{
    // TODO: implement for macOS using IOPowerSources or IOPS
    return false;
}

ENGINE_API void InitializeNetwork()
{
}

#pragma region Cocoa Implementation Specific

/// https://gist.github.com/eegrok/949034
KeyCode MapCocoaKeyCodeToKeyCode(unsigned short keyCode)
{
    switch (keyCode)
    {
        case 0x00: return KeyCode::KEY_A;
        case 0x01: return KeyCode::KEY_S;
        case 0x02: return KeyCode::KEY_D;
        case 0x03: return KeyCode::KEY_F;
        case 0x04: return KeyCode::KEY_H;
        case 0x05: return KeyCode::KEY_G;
        case 0x06: return KeyCode::KEY_Z;
        case 0x07: return KeyCode::KEY_X;
        case 0x08: return KeyCode::KEY_C;
        case 0x09: return KeyCode::KEY_V;
        // 0x0A: Section (ISO layout) - no mapping
        case 0x0B: return KeyCode::KEY_B;
        case 0x0C: return KeyCode::KEY_Q;
        case 0x0D: return KeyCode::KEY_W;
        case 0x0E: return KeyCode::KEY_E;
        case 0x0F: return KeyCode::KEY_R;
        case 0x10: return KeyCode::KEY_Y;
        case 0x11: return KeyCode::KEY_T;
        case 0x12: return KeyCode::KEY_1;
        case 0x13: return KeyCode::KEY_2;
        case 0x14: return KeyCode::KEY_3;
        case 0x15: return KeyCode::KEY_4;
        case 0x16: return KeyCode::KEY_6;
        case 0x17: return KeyCode::KEY_5;
        // 0x18: = - no mapping
        case 0x19: return KeyCode::KEY_9;
        case 0x1A: return KeyCode::KEY_7;
        case 0x1B: return KeyCode::KEY_DASH;
        case 0x1C: return KeyCode::KEY_8;
        case 0x1D: return KeyCode::KEY_0;
        // 0x1E: ] - no mapping
        case 0x1F: return KeyCode::KEY_O;
        case 0x20: return KeyCode::KEY_U;
        // 0x21: [ - no mapping
        case 0x22: return KeyCode::KEY_I;
        case 0x23: return KeyCode::KEY_P;
        case 0x24: return KeyCode::KEY_RETURN;
        case 0x25: return KeyCode::KEY_L;
        case 0x26: return KeyCode::KEY_J;
        // 0x27: ' - no mapping
        case 0x28: return KeyCode::KEY_K;
        // 0x29: ; - no mapping
        // 0x2A: \ - no mapping
        case 0x2B: return KeyCode::KEY_COMMA;
        // 0x2C: / - no mapping
        case 0x2D: return KeyCode::KEY_N;
        case 0x2E: return KeyCode::KEY_M;
        case 0x2F: return KeyCode::KEY_PERIOD;
        case 0x30: return KeyCode::KEY_TAB;
        case 0x31: return KeyCode::KEY_SPACE;
        case 0x32: return KeyCode::KEY_TILDE;
        case 0x33: return KeyCode::KEY_BACKSPACE;
        // 0x34: Enter (on Powerbook) - no mapping
        case 0x35: return KeyCode::KEY_ESCAPE;
        // 0x36: Right Cmd - no mapping
        // 0x37: Cmd (Apple) - no mapping
        case 0x38: return KeyCode::KEY_LSHIFT;
        case 0x39: return KeyCode::KEY_CAPSLOCK;
        case 0x3A: return KeyCode::KEY_LALT; // Option
        case 0x3B: return KeyCode::KEY_LCTRL;
        case 0x3C: return KeyCode::KEY_RSHIFT;
        case 0x3D: return KeyCode::KEY_RALT; // Right Option
        case 0x3E: return KeyCode::KEY_RCTRL;
        // 0x3F: Fn/Globe - no mapping
        // 0x40: F17 - no mapping
        // 0x41: Numeric Keypad . - no mapping
        // 0x43: Numeric Keypad * - no mapping
        // 0x45: Numeric Keypad + - no mapping
        // 0x47: Clear (or NumLock) - no mapping
        // 0x48: Volume Up - no mapping
        // 0x49: Volume Down - no mapping
        // 0x4A: Mute - no mapping
        // 0x4B: Numeric Keypad / - no mapping
        // 0x4C: Numeric Keypad Enter - no mapping
        // 0x4E: Numeric Keypad - - no mapping
        // 0x4F: F18 - no mapping
        // 0x50: F19 - no mapping
        // 0x51: Numeric Keypad = - no mapping
        // 0x52: Numeric Keypad 0 - no mapping
        // 0x53: Numeric Keypad 1 - no mapping
        // 0x54: Numeric Keypad 2 - no mapping
        // 0x55: Numeric Keypad 3 - no mapping
        // 0x56: Numeric Keypad 4 - no mapping
        // 0x57: Numeric Keypad 5 - no mapping
        // 0x58: Numeric Keypad 6 - no mapping
        // 0x59: Numeric Keypad 7 - no mapping
        // 0x5A: F20 - no mapping
        // 0x5B: Numeric Keypad 8 - no mapping
        // 0x5C: Numeric Keypad 9 - no mapping
        // 0x5D: Yen (JIS layout) - no mapping
        // 0x5E: Underscore (JIS layout) - no mapping
        // 0x5F: Keypad Comma/Separator (JIS layout) - no mapping
        case 0x60: return KeyCode::KEY_F5;
        case 0x61: return KeyCode::KEY_F6;
        case 0x62: return KeyCode::KEY_F7;
        case 0x63: return KeyCode::KEY_F3;
        case 0x64: return KeyCode::KEY_F8;
        case 0x65: return KeyCode::KEY_F9;
        // 0x66: Eisu (JIS layout) - no mapping
        case 0x67: return KeyCode::KEY_F11;
        // 0x68: Kana (JIS layout) - no mapping
        // 0x69: F13 - no mapping
        // 0x6A: F16 - no mapping
        // 0x6B: F14 - no mapping
        case 0x6D: return KeyCode::KEY_F10;
        // 0x6E: Menu (on PC) - no mapping
        case 0x6F: return KeyCode::KEY_F12;
        // 0x71: F15 - no mapping
        // 0x72: Help - no mapping
        // 0x73: Home - no mapping
        // 0x74: Page Up - no mapping
        // 0x75: Del (Below the Help Key) - no mapping
        case 0x76: return KeyCode::KEY_F4;
        // 0x77: End - no mapping
        case 0x78: return KeyCode::KEY_F2;
        // 0x79: Page Down - no mapping
        case 0x7A: return KeyCode::KEY_F1;
        case 0x7B: return KeyCode::KEY_LEFT;
        case 0x7C: return KeyCode::KEY_RIGHT;
        case 0x7D: return KeyCode::KEY_DOWN;
        case 0x7E: return KeyCode::KEY_UP;
        // 0x7F: Power (on PC) - no mapping
        // 0xA0: Mission Control - no mapping
        default: return KeyCode::KEY_UNKNOWN;
    }
}

static inline bool IsModifierKeyUp(
    unsigned int flags,
    unsigned int targetKeyMask,
    unsigned int otherKeyMask,
    unsigned int eitherKeyMask)
{
    const bool eitherKeyDown = (flags & eitherKeyMask) != 0;
    const bool targetKeyDown = (flags & targetKeyMask) != 0;
    const bool otherKeyDown = (flags & otherKeyMask) != 0;

    if (eitherKeyDown != (targetKeyDown || otherKeyDown))
    {
        return !eitherKeyDown;
    }

    return !targetKeyDown;
}

bool IsKeyUpEvent(NSEvent* event)
{
    if ([event type] != NSEventTypeFlagsChanged)
    {
        return [event type] == NSEventTypeKeyUp;
    }

    switch ([event keyCode])
    {
    case 0x37: // Cmd
        return IsModifierKeyUp([event modifierFlags], NX_DEVICELCMDKEYMASK,
                        NX_DEVICERCMDKEYMASK, NSEventModifierFlagCommand);
    case 0x36: // Right Cmd
        return IsModifierKeyUp([event modifierFlags], NX_DEVICERCMDKEYMASK,
                        NX_DEVICELCMDKEYMASK, NSEventModifierFlagCommand);
    case 0x39: // Caps
        return ([event modifierFlags] & NSEventModifierFlagCapsLock) == 0;
    case 0x38: // Shift
        return IsModifierKeyUp([event modifierFlags], NX_DEVICELSHIFTKEYMASK,
                        NX_DEVICERSHIFTKEYMASK, NSEventModifierFlagShift);
    case 0x3C: // Right Shift
        return IsModifierKeyUp([event modifierFlags], NX_DEVICERSHIFTKEYMASK,
                        NX_DEVICELSHIFTKEYMASK, NSEventModifierFlagShift);
    case 0x3A:  // Option
        return IsModifierKeyUp([event modifierFlags], NX_DEVICELALTKEYMASK,
                        NX_DEVICERALTKEYMASK, NSEventModifierFlagOption);
    case 0x3D:  // Right Option
        return IsModifierKeyUp([event modifierFlags], NX_DEVICERALTKEYMASK,
                        NX_DEVICELALTKEYMASK, NSEventModifierFlagOption);
    case 0x3B:  // Control
        return IsModifierKeyUp([event modifierFlags], NX_DEVICELCTLKEYMASK,
                        NX_DEVICERCTLKEYMASK, NSEventModifierFlagControl);
    case 0x3E:  // Right Control
        return IsModifierKeyUp([event modifierFlags], NX_DEVICERCTLKEYMASK,
                        NX_DEVICELCTLKEYMASK, NSEventModifierFlagControl);
    case 0x3F:  // Fn
        return ([event modifierFlags] & NSEventModifierFlagFunction) == 0;
    }

    return false;
}

#pragma endregion // Cocoa Implementation Specific

} // namespace PlatformUtils
} // namespace Hyperion
