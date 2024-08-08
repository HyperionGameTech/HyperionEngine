using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypClassProperty
    {
        internal IntPtr ptr;

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypClassProperty_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypClassProperty_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }
        
        [DllImport("hyperion", EntryPoint = "HypClassProperty_GetName")]
        private static extern void HypClassProperty_GetName(IntPtr hypClassPropertyPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypClassProperty_GetTypeID")]
        private static extern void HypClassProperty_GetTypeID(IntPtr hypClassPropertyPtr, [Out] out TypeID typeId);
    }

    public struct HypClass
    {
        public static readonly HypClass Invalid = new HypClass(IntPtr.Zero);

        private IntPtr ptr;

        internal HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
        {
            get
            {
                return ptr;
            }
            set
            {
                ptr = value;
            }
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypClass_GetName(ptr, out name);
                return name;
            }
        }

        public static HypClass? GetClass(string name)
        {
            IntPtr ptr = HypClass_GetClassByName(name);

            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            return new HypClass(ptr);
        }
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);

        
        [DllImport("hyperion", EntryPoint = "HypClass_GetName")]
        private static extern void HypClass_GetName([In] IntPtr hypClassPtr, [Out] out Name name);
    }
}