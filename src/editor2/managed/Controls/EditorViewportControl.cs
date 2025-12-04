using Avalonia.Controls;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public class EditorViewportControl : NativeControlHost
    {
        public ApplicationWindow Window { get; private set; } = null;
        public AppContextBase AppContext { get; private set; } = null;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            AppContext = AppContextBase.Instance;
            if (AppContext == null)
                throw new Exception("Failed to get AppContext from Hyperion");

            WindowOptions windowOptions = new WindowOptions();
            windowOptions.title = "EditorViewport";
            windowOptions.dimensions = new Vec2i(800, 600);
            windowOptions.flags = WindowFlags.None;

            Window = AppContext.CreateSystemWindow(windowOptions, parent.Handle);
            if (Window == null)
                throw new Exception("Failed to create engine window");

            AppContext.SetMainWindow(Window);

            if (OperatingSystem.IsWindows())
            {
                if (!(Window is Win32ApplicationWindow))
                {
                    throw new Exception("Failed to cast to Win32ApplicationWindow");
                }

                IntPtr hwnd = Window.GetHWND();

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get HWND from Hyperion");
                }

                return new PlatformHandle(hwnd, "HWND");
            }
            else if (OperatingSystem.IsMacOS())
            {
                CocoaApplicationWindow cocoaApplicationWindow = Window as CocoaApplicationWindow;
                if (cocoaApplicationWindow == null)
                {
                    throw new Exception("Failed to cast to CocoaApplicationWindow");
                }

                IntPtr nsView = cocoaApplicationWindow.GetNSView();

                if (nsView == IntPtr.Zero)
                {
                    throw new Exception("Failed to get NSView from Hyperion");
                }

                return new PlatformHandle(nsView, "NSView");
            }
            else
            {
                throw new PlatformNotSupportedException("Unsupported platform for EditorViewportControl");
            }
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            if (Window != null)
            {
                if (AppContext.GetMainWindow() == Window)
                {
                    AppContext.SetMainWindow(null);
                }
            }

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);
        }
    }
}
