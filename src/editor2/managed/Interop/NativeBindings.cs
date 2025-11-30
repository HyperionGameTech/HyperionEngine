using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public static partial class NativeBindings
    {
        [DllImport("hyperion")]
        public static extern int Hyp_Initialize(int argc, IntPtr argv);

        [DllImport("hyperion")]
        public static extern void Hyp_Shutdown();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetAppContext();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_CreateWindow(IntPtr pCtx, ref WindowOptions pWindowOptions, IntPtr parentHwnd);

        [DllImport("hyperion")]
        public static extern void Hyp_DestroyWindow(IntPtr pCtx, IntPtr pWindow);

        [DllImport("hyperion")]
        public static extern int Hyp_SetMainWindow(IntPtr pCtx, IntPtr pWindow);

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetMainWindow(IntPtr pCtx);

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetHWND(IntPtr pWindow);

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetNSView(IntPtr pWindow);

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_CreateGame(IntPtr pGameClassName);

        [DllImport("hyperion")]
        public static extern void Hyp_DestroyGame(IntPtr pGame);

        [DllImport("hyperion")]
        public static extern int Hyp_LaunchGame(IntPtr pGame);

        [DllImport("hyperion")]
        public static extern void Hyp_MainThreadUpdate();
    }
}
