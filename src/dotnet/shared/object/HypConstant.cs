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

        public TypeId TypeId
        {
            get
            {
                TypeId typeId;
                HypConstant_GetTypeId(ptr, out typeId);
                return typeId;
            }
        }

        [DllImport("hyperion", EntryPoint = "HypConstant_GetName")]
        private static extern void HypConstant_GetName([In] IntPtr constantPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypConstant_GetTypeId")]
        private static extern void HypConstant_GetTypeId([In] IntPtr constantPtr, [Out] out TypeId typeId);
    }
}