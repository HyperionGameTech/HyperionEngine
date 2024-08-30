using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum KeyCode : ushort
    {
        A = 0x41,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,

        Num0 = 0x30,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,

        F1 = 0x3A,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,

        LeftShift = 0xE1,
        LeftCtrl = 0xE0,
        LeftAlt = 0xE2,
        RightShift = 0xE5,
        RightCtrl = 0xE4,
        RightAlt = 0xE6,

        Space = 0x2C,
        Period = 0x2E,
        Return = 0x101,
        Tab = 0x102,
        Backspace = 0x103,
        CapsLock = 0x118,

        ArrowRight = 0x4F,
        ArrowLeft = 0x50,
        ArrowDown = 0x51,
        ArrowUp = 0x52
    }

    [HypClassBinding(Name="InputManager")]
    public class InputManager : HypObject
    {
        public InputManager()
        {
        }
    }
}