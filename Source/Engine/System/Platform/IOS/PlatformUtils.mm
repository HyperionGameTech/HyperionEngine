#include <SystemPch.hpp>

#include <mach-o/dyld.h>

#include <Core/Containers/String.hpp>

#include <Input/Keyboard.hpp>

#import <UIKit/UIKit.h>

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
    [[UIDevice currentDevice] setBatteryMonitoringEnabled:YES];

    const UIDeviceBatteryState state = [UIDevice currentDevice].batteryState;

    return state == UIDeviceBatteryStateUnplugged;
}

void PumpSystemEvents()
{
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
}

/// Maps UIKeyboardHIDUsage values (USB HID keyboard usage page) to engine KeyCode.
/// Reference: https://developer.apple.com/documentation/uikit/uikeyboardhidusage
KeyCode MapIOSKeyCodeToKeyCode(unsigned short hidUsage)
{
    switch (hidUsage)
    {
        case 0x04: return KeyCode::KEY_A;
        case 0x05: return KeyCode::KEY_B;
        case 0x06: return KeyCode::KEY_C;
        case 0x07: return KeyCode::KEY_D;
        case 0x08: return KeyCode::KEY_E;
        case 0x09: return KeyCode::KEY_F;
        case 0x0A: return KeyCode::KEY_G;
        case 0x0B: return KeyCode::KEY_H;
        case 0x0C: return KeyCode::KEY_I;
        case 0x0D: return KeyCode::KEY_J;
        case 0x0E: return KeyCode::KEY_K;
        case 0x0F: return KeyCode::KEY_L;
        case 0x10: return KeyCode::KEY_M;
        case 0x11: return KeyCode::KEY_N;
        case 0x12: return KeyCode::KEY_O;
        case 0x13: return KeyCode::KEY_P;
        case 0x14: return KeyCode::KEY_Q;
        case 0x15: return KeyCode::KEY_R;
        case 0x16: return KeyCode::KEY_S;
        case 0x17: return KeyCode::KEY_T;
        case 0x18: return KeyCode::KEY_U;
        case 0x19: return KeyCode::KEY_V;
        case 0x1A: return KeyCode::KEY_W;
        case 0x1B: return KeyCode::KEY_X;
        case 0x1C: return KeyCode::KEY_Y;
        case 0x1D: return KeyCode::KEY_Z;
        case 0x1E: return KeyCode::KEY_1;
        case 0x1F: return KeyCode::KEY_2;
        case 0x20: return KeyCode::KEY_3;
        case 0x21: return KeyCode::KEY_4;
        case 0x22: return KeyCode::KEY_5;
        case 0x23: return KeyCode::KEY_6;
        case 0x24: return KeyCode::KEY_7;
        case 0x25: return KeyCode::KEY_8;
        case 0x26: return KeyCode::KEY_9;
        case 0x27: return KeyCode::KEY_0;
        case 0x28: return KeyCode::KEY_RETURN;
        case 0x29: return KeyCode::KEY_ESCAPE;
        case 0x2A: return KeyCode::KEY_BACKSPACE;
        case 0x2B: return KeyCode::KEY_TAB;
        case 0x2C: return KeyCode::KEY_SPACE;
        case 0x2D: return KeyCode::KEY_DASH;
        case 0x2E: return KeyCode::KEY_PERIOD;
        case 0x2F: return KeyCode::KEY_TILDE;
        case 0x33: return KeyCode::KEY_COMMA;
        case 0x35: return KeyCode::KEY_TILDE;
        case 0x36: return KeyCode::KEY_COMMA;
        case 0x38: return KeyCode::KEY_LALT;
        case 0x39: return KeyCode::KEY_CAPSLOCK;
        case 0x4F: return KeyCode::KEY_RIGHT;
        case 0x50: return KeyCode::KEY_LEFT;
        case 0x51: return KeyCode::KEY_DOWN;
        case 0x52: return KeyCode::KEY_UP;
        case 0xE0: return KeyCode::KEY_LCTRL;
        case 0xE1: return KeyCode::KEY_LSHIFT;
        case 0xE2: return KeyCode::KEY_LALT;
        case 0xE4: return KeyCode::KEY_RCTRL;
        case 0xE5: return KeyCode::KEY_RSHIFT;
        case 0xE6: return KeyCode::KEY_RALT;
        case 0x3A: return KeyCode::KEY_F1;
        case 0x3B: return KeyCode::KEY_F2;
        case 0x3C: return KeyCode::KEY_F3;
        case 0x3D: return KeyCode::KEY_F4;
        case 0x3E: return KeyCode::KEY_F5;
        case 0x3F: return KeyCode::KEY_F6;
        case 0x40: return KeyCode::KEY_F7;
        case 0x41: return KeyCode::KEY_F8;
        case 0x42: return KeyCode::KEY_F9;
        case 0x43: return KeyCode::KEY_F10;
        case 0x44: return KeyCode::KEY_F11;
        case 0x45: return KeyCode::KEY_F12;
        default:   return KeyCode::KEY_UNKNOWN;
    }
}

bool IsHardwareKeyboardAvailable()
{
    return false;
}

} // namespace PlatformUtils
} // namespace Hyperion
