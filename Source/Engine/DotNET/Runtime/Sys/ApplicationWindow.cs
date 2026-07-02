using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WindowFlags")]
    [Flags]
    public enum WindowFlags : uint
    {
        None = 0,
        Headless = 0x1,
        NoGfx = 0x2,
        HighDpi = 0x4,
        EventsPolling = 0x8
    }

    [ClassBinding(Name = "WindowOptions")]
    [StructLayout(LayoutKind.Explicit, Size = 280, Pack = 8)]
    public struct WindowOptions
    {
        // maps to char[256]
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        [FieldOffset(0)]
        public string title;

        [FieldOffset(256)]
        public Vec2i dimensions;

        [FieldOffset(264)]
        public WindowFlags flags;

        [FieldOffset(272)]
        public IntPtr parentHwnd;
    }

    [ClassBinding(Name = "ApplicationWindow")]
    public class ApplicationWindow : ObjectBase
    {
        public ApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "Win32ApplicationWindow", Condition = "IsWindows")]
    public class Win32ApplicationWindow : ApplicationWindow
    {
        public Win32ApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "CocoaApplicationWindow", Condition = "IsMacOS")]
    public class CocoaApplicationWindow : ApplicationWindow
    {
        public CocoaApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "AndroidApplicationWindow", Condition = "IsAndroid")]
    public class AndroidApplicationWindow : ApplicationWindow
    {
        public AndroidApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "iOSApplicationWindow", Condition = "IsIOS")]
    public class iOSApplicationWindow : ApplicationWindow
    {
        public iOSApplicationWindow()
        {
        }
    }
}
