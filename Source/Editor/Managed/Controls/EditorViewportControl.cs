using Avalonia;
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

        public static readonly StyledProperty<EditorViewportType> ViewportTypeProperty =
            AvaloniaProperty.Register<EditorViewportControl, EditorViewportType>(nameof(ViewportType), EditorViewportType.Perspective);

        public EditorViewportType ViewportType
        {
            get => GetValue(ViewportTypeProperty);
            set => SetValue(ViewportTypeProperty, value);
        }

        // the perspective viewport owns the app's main window; the others are presented alongside it
        public bool IsMainViewport => ViewportType == EditorViewportType.Perspective;

        public ApplicationWindow? Window { get; private set; } = null;
        public AppContextBase? AppContext { get; private set; } = null;
        public EditorViewport? Viewport { get; private set; } = null;

        private DelegateHandler? _gameLaunchedHandler;

        static EditorViewportControl()
        {
            if (OperatingSystem.IsLinux())
            {
                PointerPressedEvent.AddClassHandler<Window>(
                    (window, _) =>
                    {
                        if (window.TryGetPlatformHandle() is { HandleDescriptor: "XID" } handle)
                        {
                            X11Interop.ReclaimKeyboardFocus((ulong)handle.Handle);
                        }
                    },
                    Avalonia.Interactivity.RoutingStrategies.Tunnel,
                    handledEventsToo: true);
            }
        }

        public EditorViewportControl()
        {
            Focusable = true;
            IsHitTestVisible = true;

            Viewport = new EditorViewport();
        }

        private IPlatformHandle? _nativeParentHandle;

        /// <summary>
        /// Hides or shows the viewport's native container window without destroying it.
        /// Native child windows always render above Avalonia overlays (airspace), so the
        /// whole container is hidden while docking drag indicators or pinned panel
        /// previews are active.
        /// </summary>
        public void SetNativeVisibility(bool visible)
        {
            if (_nativeParentHandle == null)
            {
                return;
            }

            if (OperatingSystem.IsWindows())
            {
                const int SW_HIDE = 0;
                const int SW_SHOW = 5;
                WinInterop.ShowWindow(_nativeParentHandle.Handle, visible ? SW_SHOW : SW_HIDE);
            }

            // macOS/Linux: hiding without destroying the native view is not wired up yet.
        }

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            _nativeParentHandle = parent;

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

            Viewport.SetViewportType(ViewportType);

            InitEditorViewport(Viewport);

            WindowOptions windowOptions = new WindowOptions();
            windowOptions.title = "EditorViewport";
            windowOptions.dimensions = new Vec2i(width, height);
            windowOptions.flags = WindowFlags.None;
            windowOptions.parentHwnd = parent.Handle;

            Window = Viewport.CreateViewportWindow(windowOptions, IsMainViewport);

            if (Window == null)
            {
                Logger.Log(LogLevel.Error, "Failed to create the editor viewport window.");
            }

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
#if HYP_MACOS // CocoaApplicationWindow's bindings only exist in a macOS build
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
#endif
            else if (OperatingSystem.IsLinux())
            {
                if (!(Window is X11ApplicationWindow))
                {
                    throw new Exception("Failed to cast to X11ApplicationWindow");
                }

                // X11 window id (XID)
                IntPtr xid = Window.GetHWND();

                if (xid == IntPtr.Zero)
                {
                    throw new Exception("Failed to get X11 window from Hyperion");
                }

                return new PlatformHandle(xid, "XID");
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
                else
                {
                    AppContext.RemovePresentWindow(Window);
                }

                Window = null;
            }

            if (Viewport != null)
            {
                Viewport.ReleaseViewportWindow();

                EngineManager.UnregisterViewport(Viewport);
            }

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);

            if (Window == null)
            {
                return;
            }

            int width = (int)e.NewSize.Width;
            int height = (int)e.NewSize.Height;

            if (width <= 0 || height <= 0)
            {
                return;
            }

#if HYP_MACOS
            if (OperatingSystem.IsMacOS() && Window is CocoaApplicationWindow cocoaWindow)
            {
                IntPtr nsView = cocoaWindow.GetNSView();
                if (nsView != IntPtr.Zero)
                {
                    MacInterop.ResizeEmbeddedView(nsView, width, height);
                }
            }
#endif
            // else if (OperatingSystem.IsWindows() && Window is Win32ApplicationWindow win32Window)
            // {
            //     IntPtr hwnd = win32Window.GetHWND();
            //     if (hwnd != IntPtr.Zero)
            //     {
            //         const uint SWP_NOZORDER = 0x0004;
            //         const uint SWP_NOACTIVATE = 0x0010;
            //         WinInterop.SetWindowPos(hwnd, IntPtr.Zero, 0, 0, width, height,
            //             SWP_NOZORDER | SWP_NOACTIVATE);
            //     }
            // }
        }

        private static class MacInterop
        {
            [DllImport("hyperion", EntryPoint = "Hyp_CocoaWindow_ResizeEmbeddedView")]
            public static extern void ResizeEmbeddedView(IntPtr nsView, int width, int height);
        }

        private static class X11Interop
        {
            [DllImport("hyperion", EntryPoint = "Hyp_X11_ReclaimKeyboardFocus")]
            public static extern void ReclaimKeyboardFocus(ulong topLevelWindow);
        }

        private static class WinInterop
        {
            [DllImport("user32.dll")]
            public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags);

            [DllImport("user32.dll")]
            public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        }

        void InitEditorViewport(EditorViewport viewport)
        {
            EngineManager.RegisterViewport(viewport);
        }
    }
}
