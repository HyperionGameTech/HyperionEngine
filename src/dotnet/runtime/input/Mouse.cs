using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MouseButtonKey")]
    public enum MouseButtonKey : uint
    {
        Invalid = ~0u,
        Left = 0,
        Middle,
        Right,
        Max
    }

    [ClassBinding(Name="MouseButtonState")]
    [Flags]
    public enum MouseButtonState : uint
    {
        None = 0x0,
        Left = (uint)(1 << (int)MouseButtonKey.Left),
        Middle = (uint)(1 << (int)MouseButtonKey.Middle),
        Right = (uint)(1 << (int)MouseButtonKey.Right)
    }

    [ClassBinding(Name="MouseEvent")]
    [StructLayout(LayoutKind.Explicit, Size = 56)]
    public struct MouseEvent
    {
        [FieldOffset(0)]
        private Ptr<InputManager> inputManager;

        [FieldOffset(8)]
        private Vec2f position;

        [FieldOffset(16)]
        private Vec2f previousPosition;

        [FieldOffset(24)]
        private Vec2i absolutePosition;

        [FieldOffset(32)]
        private MouseButtonState mouseButtons;

        [FieldOffset(36)]
        private Vec2i wheel;

        [FieldOffset(44)]
        private bool isDown;
    }
}