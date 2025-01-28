using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct HypConstant
    {
        public static readonly HypConstant Invalid = new HypConstant(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypConstant(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypConstant_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypConstant_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }

        [DllImport("hyperion", EntryPoint = "HypConstant_GetName")]
        private static extern void HypConstant_GetName([In] IntPtr constantPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypConstant_GetTypeID")]
        private static extern void HypConstant_GetTypeID([In] IntPtr constantPtr, [Out] out TypeID typeId);
    }
}