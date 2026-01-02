using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Platform;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public class EditorViewportControl : NativeControlHost
    {
        private const int DefaultWidth = 800;
        private const int DefaultHeight = 600;

        public ApplicationWindow? Window { get; private set; } = null;
        public AppContextBase? AppContext { get; private set; } = null;
        public EditorViewport? Viewport { get; private set; } = null;

        private DelegateHandler? _gameLaunchedHandler;

        public EditorViewportControl()
        {
            Focusable = true;
            IsHitTestVisible = true;

            Viewport = new EditorViewport();
        }

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            AppContext = AppContextBase.Instance;
            if (AppContext == null)
                throw new Exception("Failed to get AppContext from Hyperion");

            int width = (int)Width;
            int height = (int)Height;

            if (width <= 0 || height <= 0)
            {
                width = DefaultWidth;
                height = DefaultHeight;
            }

            InitEditorViewport(Viewport);

            WindowOptions windowOptions = new WindowOptions();
            windowOptions.title = "EditorViewport";
            windowOptions.dimensions = new Vec2i(width, height);
            windowOptions.flags = WindowFlags.None;
            windowOptions.parentHwnd = parent.Handle;

            Window = Viewport.CreateViewportWindow(windowOptions);
            
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
                CocoaApplicationWindow? cocoaApplicationWindow = Window as CocoaApplicationWindow;
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
            _gameLaunchedHandler?.Remove();
            _gameLaunchedHandler = null;

            if (Window != null)
            {
                Debug.Assert(AppContext != null);

                if (AppContext.GetMainWindow() == Window)
                {
                    AppContext.SetMainWindow(null);
                }
            }

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            Logger.Log(LogType.Info, $"EditorViewportControl OnSizeChanged: New Size = {e.NewSize.Width} x {e.NewSize.Height}");

            base.OnSizeChanged(e);

            if (Window != null)
            {
                int width = (int)e.NewSize.Width;
                int height = (int)e.NewSize.Height;

                if (width <= 0 || height <= 0)
                {
                    return;
                }

                // Window.SetSize(new Vec2i(width, height));
            }
        }

        void InitEditorViewport(EditorViewport viewport)
        {
            EngineManager.RegisterViewport(viewport);
        }
    }
}
