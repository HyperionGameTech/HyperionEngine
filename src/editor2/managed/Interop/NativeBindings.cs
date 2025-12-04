using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public delegate void InitFromManagedDelegate();

    public static partial class NativeBindings
    {
        [DllImport("hyperion")]
        public static extern int Hyp_Initialize(int argc, IntPtr argv);

        [DllImport("hyperion")]
        public static extern void Hyp_Shutdown();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetAppContext();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_CreateGame(IntPtr pGameClassName);

        [DllImport("hyperion")]
        public static extern void Hyp_DestroyGame(IntPtr pGame);

        [DllImport("hyperion")]
        public static extern int Hyp_LaunchGame(IntPtr pGame);

        [DllImport("hyperion")]
        public static extern void Hyp_MainThreadUpdate();

        [DllImport("hyperion")]
        public static unsafe extern void Hyp_SetInitFromManagedCallback(delegate* unmanaged<void> callback);
    }
}
