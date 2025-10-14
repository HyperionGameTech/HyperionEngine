using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Wraps a pointer to a native (C++) TypeInfo (see core/utilities/TypeInfoFwd.hpp)
    /// </summary>

    [StructLayout(LayoutKind.Sequential)]
    public struct TypeInfo
    {
        private IntPtr ptr;

        public TypeInfo()
        {
            ptr = IntPtr.Zero;
        }

        public TypeInfo(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsNull
        {
            get
            {
                return ptr == IntPtr.Zero;
            }
        }
    }
}