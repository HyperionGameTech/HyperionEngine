using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct StaticField
    {
        public static readonly StaticField Invalid = new StaticField(IntPtr.Zero);

        internal IntPtr ptr;

        internal StaticField(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                StaticField_GetName(ptr, out name);
                return name;
            }
        }

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                StaticField_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        [DllImport("hyperion", EntryPoint = "StaticField_GetName")]
        private static extern void StaticField_GetName([In] IntPtr constantPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "StaticField_GetTypeId")]
        private static extern void StaticField_GetTypeId([In] IntPtr constantPtr, [Out] out TypeId typeId);
    }
}