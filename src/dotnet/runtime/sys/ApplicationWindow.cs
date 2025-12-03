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
    [StructLayout(LayoutKind.Sequential)]
    public struct WindowOptions
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string title;
        public int width;
        public int height;
        public WindowFlags flags;
    }

    [ClassBinding(Name = "ApplicationWindow")]
    public class ApplicationWindow : ObjectBase
    {
        public ApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "Win32ApplicationWindow")]
    public class Win32ApplicationWindow : ApplicationWindow
    {
        public Win32ApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "CocoaApplicationWindow")]
    public class CocoaApplicationWindow : ApplicationWindow
    {
        public CocoaApplicationWindow()
        {
        }
    }

    [ClassBinding(Name = "SDLApplicationWindow")]
    public class SDLApplicationWindow : ApplicationWindow
    {
        public SDLApplicationWindow()
        {
        }
    }
}