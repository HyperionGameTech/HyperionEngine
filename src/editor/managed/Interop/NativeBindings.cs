using System;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    public unsafe delegate void InitFromManagedDelegate(ManagedDelegates* pManagedDelegates);

    public unsafe delegate int InitializeRuntimeDelegate();
    public unsafe delegate int InitializeAssemblyDelegate(IntPtr* pAssemblyGuid, IntPtr pAssembly, IntPtr pFilePath, int isCoreAssembly);
    public unsafe delegate void UnloadAssemblyDelegate(IntPtr pAssemblyGuid, IntPtr pResult);

    public unsafe struct ManagedDelegates
    {
        public delegate* unmanaged<int> initializeRuntime;
        public delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int> initializeAssembly;
        public delegate* unmanaged<IntPtr, IntPtr, void> unloadAssembly;
    }

    public static partial class NativeBindings
    {
        [DllImport("hyperion")]
        public static extern int Hyp_Initialize(int argc, IntPtr argv);

        [DllImport("hyperion")]
        public static extern void Hyp_LaunchThreads();

        [DllImport("hyperion")]
        public static extern void Hyp_Shutdown();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_GetAppContext();

        [DllImport("hyperion")]
        public static extern IntPtr Hyp_CreateGame(IntPtr pGameClassName);

        [DllImport("hyperion")]
        public static extern void Hyp_DestroyGame(IntPtr pGame);

        [DllImport("hyperion")]
        public static extern void Hyp_MainThreadUpdate();

        [DllImport("hyperion")]
        public static unsafe extern void Hyp_SetInitFromManagedCallback(delegate* unmanaged<ManagedDelegates*, void> callback);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void LogCallbackDelegate([MarshalAs(UnmanagedType.LPStr)] string channel, int level, double timestamp, [MarshalAs(UnmanagedType.LPStr)] string message);

        [DllImport("hyperion")]
        public static extern void Editor_RegisterLogCallback(LogCallbackDelegate callback);

        [DllImport("hyperion")]
        public static extern void Editor_ExecuteConsoleCommand([MarshalAs(UnmanagedType.LPStr)] string command);
    }
}
