using Avalonia.Controls;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public class EditorViewport : NativeControlHost
    {
        private IntPtr m_ctx = IntPtr.Zero;
        private IntPtr m_window = IntPtr.Zero;

        private const bool UseExistingWindow = false;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            // Ensure engine is initialized (should be done in App.axaml.cs, but safe to check)
            if (!EngineManager.IsInitialized)
            {
                EngineManager.Initialize();
            }

            // Create window
            m_ctx = NativeBindings.Hyp_GetAppContext();

            if (m_ctx == IntPtr.Zero)
            {
                throw new Exception("Failed to get AppContext from Hyperion");
            }

            if (UseExistingWindow)
            {
                m_window = NativeBindings.Hyp_GetMainWindow(m_ctx);
                if (m_window == IntPtr.Zero)
                {
                    throw new Exception("Failed to get main window from Hyperion");
                }
            }
            else
            {
                WindowOptions windowOptions = new WindowOptions();
                windowOptions.title = "EditorViewport";
                windowOptions.width = 800;
                windowOptions.height = 600;
                windowOptions.flags = WindowFlags.None;

                m_window = NativeBindings.Hyp_CreateWindow(m_ctx, ref windowOptions, parent.Handle);

                if (m_window == IntPtr.Zero)
                {
                    throw new Exception("Failed to create window");
                }

                if (NativeBindings.Hyp_SetMainWindow(m_ctx, m_window) == 0)
                {
                    throw new Exception("Failed to set main window");
                }
            }

            EngineManager.InitializeEditor();

            if (OperatingSystem.IsWindows())
            {
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(m_window);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get HWND from Hyperion");
                }

                return new PlatformHandle(hwnd, "HWND");
            }
            else if (OperatingSystem.IsMacOS())
            {
                IntPtr nsView = NativeBindings.Hyp_GetNSView(m_window);

                if (nsView == IntPtr.Zero)
                {
                    throw new Exception("Failed to get NSView from Hyperion");
                }

                return new PlatformHandle(nsView, "NSView");
            }
            else
            {
                // Linux/X11
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(m_window);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get window handle from Hyperion");
                }

                return new PlatformHandle(hwnd, "XID");
            }
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            if (m_window != IntPtr.Zero && m_ctx != IntPtr.Zero)
            {
                NativeBindings.Hyp_DestroyWindow(m_ctx, m_window);
                m_window = IntPtr.Zero;
            }

            // Do not shutdown engine here, it is handled by EngineManager in App.axaml.cs

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);
        }
    }
}
