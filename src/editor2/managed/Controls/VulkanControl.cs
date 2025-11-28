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
        [DllImport("hyperion")]
        private static extern int Hyp_Initialize(int argc, IntPtr argv);

        [DllImport("hyperion")]
        private static extern void Hyp_Shutdown();

        [DllImport("hyperion")]
        private static extern IntPtr Hyp_GetAppContext();

        [DllImport("hyperion")]
        private static extern IntPtr Hyp_CreateWindow(IntPtr pCtx, ref WindowOptions pWindowOptions, IntPtr parentHwnd);

        [DllImport("hyperion")]
        private static extern void Hyp_DestroyWindow(IntPtr pCtx, IntPtr pWindow);

        [DllImport("hyperion")]
        private static extern int Hyp_SetMainWindow(IntPtr pCtx, IntPtr pWindow);

        [DllImport("hyperion")]
        private static extern IntPtr Hyp_GetHWND(IntPtr pWindow);

        [DllImport("hyperion")]
        private static extern IntPtr Hyp_CreateGame(IntPtr pGameClassName);

        [DllImport("hyperion")]
        private static extern void Hyp_DestroyGame(IntPtr pGame);

        [DllImport("hyperion")]
        private static extern int Hyp_LaunchGame(IntPtr pGame);

        private IntPtr mCtx = IntPtr.Zero;
        private IntPtr mWindow = IntPtr.Zero;
        private IntPtr mGame = IntPtr.Zero;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            // create argv for Hyp_Initialize if needed
            List<string> args = [
                Environment.ProcessPath ?? "",
                "-Headless=true",
                "-Detached=true"
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

                if (Hyp_Initialize(argc, argv) == 0)
                {
                    throw new Exception("Failed to initialize Hyperion Engine. Hyp_Initialize returned false.");
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

            mCtx = Hyp_GetAppContext();

            if (mCtx == IntPtr.Zero)
            {
                throw new Exception("Failed to get AppContext from Hyperion");
            }

            WindowOptions windowOptions = new WindowOptions();
            windowOptions.title = "EditorViewport";
            windowOptions.width = (int)Bounds.Width > 0 ? (int)Bounds.Width : 800;
            windowOptions.height = (int)Bounds.Height > 0 ? (int)Bounds.Height : 600;
            windowOptions.flags = WindowFlags.Headless;

            mWindow = Hyp_CreateWindow(mCtx, ref windowOptions, parent.Handle);

            if (mWindow == IntPtr.Zero)
            {
                throw new Exception("Failed to create window");
            }

            if (Hyp_SetMainWindow(mCtx, mWindow) == 0)
            {
                throw new Exception("Failed to set main window");
            }

            IntPtr hwnd = Hyp_GetHWND(mWindow);

            if (hwnd == IntPtr.Zero)
            {
                throw new Exception("Failed to get HWND from Hyperion");
            }

            IntPtr pStr = Marshal.StringToHGlobalAnsi("HyperionEditor");
            mGame = Hyp_CreateGame(pStr);
            Marshal.FreeHGlobal(pStr);

            // @TODO share one game instance between all viewports
            if (mGame == IntPtr.Zero)
            {
                throw new Exception("Failed to create HyperionEditor instance");
            }

            if (Hyp_LaunchGame(mGame) == 0)
            {
                throw new Exception("Failed to launch HyperionEditor instance");
            }

            if (OperatingSystem.IsWindows())
            {
                return new PlatformHandle(hwnd, "HWND");
            }
            else
            {
                return new PlatformHandle(hwnd, "XID");
            }
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            if (mGame != IntPtr.Zero)
            {
                Hyp_DestroyGame(mGame);
                mGame = IntPtr.Zero;
            }

            Hyp_DestroyWindow(mCtx, mWindow);
            mWindow = IntPtr.Zero;

            Hyp_Shutdown();

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
