using Avalonia.Controls;
using Avalonia.Platform;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text;
using System.Threading.Tasks;

namespace Hyperion.Editor
{
    [Flags]
    public enum WindowFlags : uint
    {
        None = 0,
        Headless = 0x1,
        NoGfx = 0x2,
        HighDpi = 0x4
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WindowOptions
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string title;
        public int width;
        public int height;
        public WindowFlags flags;
    }

    public class VulkanViewport : NativeControlHost
    {
        private IntPtr mCtx = IntPtr.Zero;
        private IntPtr mWindow = IntPtr.Zero;
        private IntPtr mGame = IntPtr.Zero;

        private const bool UseExistingWindow = false;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            // create argv for NativeBindings.Hyp_Initialize if needed
            List<string> args = [
                Environment.ProcessPath ?? "",
                "-Headless=true",
                "-Detached=true",
                "-RenderOnMainThread=true"
            ];

            int argc = args.Count;
            IntPtr argv = IntPtr.Zero;
            IntPtr[] argsPtrs = new IntPtr[argc];

            try
            {
                for (int i = 0; i < argc; i++)
                {
                    argsPtrs[i] = Marshal.StringToHGlobalAnsi(args[i]);
                }

                argv = Marshal.AllocHGlobal(IntPtr.Size * argc);

                for (int i = 0; i < argc; i++)
                {
                    Marshal.WriteIntPtr(argv, i * IntPtr.Size, argsPtrs[i]);
                }

                if (NativeBindings.Hyp_Initialize(argc, argv) == 0)
                {
                    throw new Exception("Failed to initialize Hyperion Engine. NativeBindings.Hyp_Initialize returned false.");
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during Hyperion Engine initialization: " + ex.Message);
            }
            finally
            {
                for (int i = 0; i < argc; i++)
                {
                    if (argsPtrs[i] != IntPtr.Zero)
                    {
                        Marshal.FreeHGlobal(argsPtrs[i]);
                    }
                }

                if (argv != IntPtr.Zero)
                    Marshal.FreeHGlobal(argv);
            }

            mCtx = NativeBindings.Hyp_GetAppContext();

            if (mCtx == IntPtr.Zero)
            {
                throw new Exception("Failed to get AppContext from Hyperion");
            }

            if (UseExistingWindow)
            {
                mWindow = NativeBindings.Hyp_GetMainWindow(mCtx);
                if (mWindow == IntPtr.Zero)
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

                mWindow = NativeBindings.Hyp_CreateWindow(mCtx, ref windowOptions, parent.Handle);

                if (mWindow == IntPtr.Zero)
                {
                    throw new Exception("Failed to create window");
                }

                if (NativeBindings.Hyp_SetMainWindow(mCtx, mWindow) == 0)
                {
                    throw new Exception("Failed to set main window");
                }
            }

            IntPtr pStr = Marshal.StringToHGlobalAnsi("HyperionEditor");
            mGame = NativeBindings.Hyp_CreateGame(pStr);
            Marshal.FreeHGlobal(pStr);

            // @TODO share one game instance between all viewports
            if (mGame == IntPtr.Zero)
            {
                throw new Exception("Failed to create HyperionEditor instance");
            }

            if (NativeBindings.Hyp_LaunchGame(mGame) == 0)
            {
                throw new Exception("Failed to launch HyperionEditor instance");
            }

            if (OperatingSystem.IsWindows())
            {
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(mWindow);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get HWND from Hyperion");
                }

                return new PlatformHandle(hwnd, "HWND");
            }
            else if (OperatingSystem.IsMacOS())
            {
                IntPtr nsView = NativeBindings.Hyp_GetNSView(mWindow);

                if (nsView == IntPtr.Zero)
                {
                    throw new Exception("Failed to get NSView from Hyperion");
                }

                return new PlatformHandle(nsView, "NSView");
            }
            else
            {
                // Linux/X11
                IntPtr hwnd = NativeBindings.Hyp_GetHWND(mWindow);

                if (hwnd == IntPtr.Zero)
                {
                    throw new Exception("Failed to get window handle from Hyperion");
                }

                return new PlatformHandle(hwnd, "XID");
            }
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            if (mGame != IntPtr.Zero)
            {
                NativeBindings.Hyp_DestroyGame(mGame);
                mGame = IntPtr.Zero;
            }

            NativeBindings.Hyp_DestroyWindow(mCtx, mWindow);
            mWindow = IntPtr.Zero;

            NativeBindings.Hyp_Shutdown();

            base.DestroyNativeControlCore(control);
        }

        protected override void OnSizeChanged(Avalonia.Controls.SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);

            //if (nativeWindowHandle != IntPtr.Zero)
            //{
            //ResizeEngineWindow(nativeWindowHandle, (int)e.NewSize.Width, (int)e.NewSize.Height);
            //}
        }
    }
}
