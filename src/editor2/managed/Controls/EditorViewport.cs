using Avalonia.Controls;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public class EditorViewport : NativeControlHost
    {
        public IntPtr Window { get; private set; } = IntPtr.Zero;
        public IntPtr AppContext { get; private set; } = IntPtr.Zero;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            AppContext = NativeBindings.Hyp_GetAppContext();
            if (AppContext == IntPtr.Zero)
                throw new Exception("Failed to get AppContext from Hyperion");

            WindowOptions windowOptions = new WindowOptions();
            windowOptions.title = "EditorViewport";
            windowOptions.width = 800;
            windowOptions.height = 600;
            windowOptions.flags = WindowFlags.None;

            Window = NativeBindings.Hyp_CreateWindow(AppContext, ref windowOptions, parent.Handle);
            if (Window == IntPtr.Zero)
                throw new Exception("Failed to create engine window");

            if (NativeBindings.Hyp_SetMainWindow(AppContext, Window) == 0)
                throw new Exception("Failed to set main window");

            if (Window == IntPtr.Zero)
                throw new Exception("EditorViewport requires a valid engine window handle provided externally.");

            if (OperatingSystem.IsWindows())
            {
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(Window);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get HWND from Hyperion");
                }

                return new PlatformHandle(hwnd, "HWND");
            }
            else if (OperatingSystem.IsMacOS())
            {
                IntPtr nsView = NativeBindings.Hyp_GetNSView(Window);

                if (nsView == IntPtr.Zero)
                {
                    throw new Exception("Failed to get NSView from Hyperion");
                }

                return new PlatformHandle(nsView, "NSView");
            }
            else
            {
                // Linux/X11
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(Window);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get window handle from Hyperion");
                }

                return new PlatformHandle(hwnd, "XID");
            }
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            if (Window != IntPtr.Zero)
            {
                NativeBindings.Hyp_DestroyWindow(AppContext, Window);

                Window = IntPtr.Zero;
            }

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);
        }
    }
}
