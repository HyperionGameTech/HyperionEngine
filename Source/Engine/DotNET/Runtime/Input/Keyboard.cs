using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "KeyCode")]
    public enum KeyCode : ushort
    {
        Unknown = ushort.MaxValue,

        A = 97,
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

        Num0 = 48,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,

        F1 = 290,
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

        LeftCtrl = 224,
        LeftShift = 225,
        LeftAlt = 226,
        RightCtrl = 228,
        RightShift = 229,
        RightAlt = 230,

        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Dash = 45,
        Period = 46,
        Slash = 47,
        Semicolon = 59,
        Equals = 61,
        Return = 13,
        Tab = 258,
        Backspace = 8,
        CapsLock = 280,
        Tilde = 96,

        ArrowRight = 79,
        ArrowLeft = 80,
        ArrowDown = 81,
        ArrowUp = 82,

        Escape = 27
    }

    [ClassBinding(Name = "KeyboardEvent")]
    [StructLayout(LayoutKind.Sequential)]
    public struct KeyboardEvent
    {
        private IntPtr _baseEvent;
        private Ptr<InputManager> inputManager;
        private KeyCode keyCode;
    }
}
