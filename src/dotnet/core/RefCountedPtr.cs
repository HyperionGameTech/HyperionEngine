using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Wrapper for RefCountedPtr.hpp from the core library
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr(IntPtr.Zero);

        private IntPtr ptr;

        public RefCountedPtr(IntPtr ptr)
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
    }
}
