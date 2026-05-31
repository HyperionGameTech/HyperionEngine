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
    [StructLayout(LayoutKind.Sequential)]
    public struct MouseEvent
    {
        private IntPtr _baseEvent;
        public Ptr<InputManager> inputManager;
        public Vec2f relativePos;
        public Vec2f relativePrevPos;
        public Vec2f absolutePos;
        private Vec2f absolutePrevPos;
        public MouseButtonState mouseButtons;
        public Vec2i wheel;
        public bool isDown;
    }
}