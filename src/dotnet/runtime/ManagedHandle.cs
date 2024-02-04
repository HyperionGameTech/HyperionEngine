using System;
using System.Runtime.InteropServices;

namespace Hyperion {
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct ManagedHandle
    {
        [MarshalAs(UnmanagedType.U4)]
        public uint id;

        public bool Valid
        {
            get
            {
                return id != 0;
            }
        }

        public void IncRef(TypeID typeId)
        {
            ManagedHandle_IncRef(typeId, this);
        }

        public void DecRef(TypeID typeId)
        {
            ManagedHandle_DecRef(typeId, this);

            id = 0;
        }

        [DllImport("libhyperion", EntryPoint = "ManagedHandle_IncRef")]
        private static extern void ManagedHandle_IncRef(TypeID type_id, ManagedHandle handle);

        [DllImport("libhyperion", EntryPoint = "ManagedHandle_DecRef")]
        private static extern void ManagedHandle_DecRef(TypeID type_id, ManagedHandle handle);
    }
}