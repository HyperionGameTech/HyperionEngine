using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypField
    {
        public static readonly HypField Invalid = new HypField(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypField(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypField_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypField_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }

        public uint Offset
        {
            get
            {
                return HypField_GetOffset(ptr);
            }
        }

        [DllImport("hyperion", EntryPoint = "HypField_GetName")]
        private static extern void HypField_GetName([In] IntPtr fieldPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypField_GetTypeID")]
        private static extern void HypField_GetTypeID([In] IntPtr fieldPtr, [Out] out TypeID typeId);

        [DllImport("hyperion", EntryPoint = "HypField_GetOffset")]
        private static extern uint HypField_GetOffset([In] IntPtr fieldPtr);
    }
}