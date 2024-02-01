using System;
using System.Runtime.InteropServices;

namespace Hyperion {
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ManagedHandle
    {
        [MarshalAs(UnmanagedType.U4)]
        public uint type_id;

        [MarshalAs(UnmanagedType.U4)]
        public uint id;

        public void Dispose()
        {
            ManagedHandle_Dispose(this);

            type_id = 0;
            id = 0;
        }

        [DllImport("libhyperion", EntryPoint = "ManagedHandle_Dispose")]
        private static extern void ManagedHandle_Dispose(ManagedHandle handle);

        [DllImport("libhyperion", EntryPoint = "ManagedHandle_GetID")]
        private static extern uint ManagedHandle_GetID(ManagedHandle handle);
    }
}