using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// Wrapper struct corresponding to /core/memory/Pimpl.hpp in the C++ code
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct PimplPtr
    {
        [FieldOffset(0)]
        private IntPtr ptr;

        [FieldOffset(8)]
        private IntPtr deleter;

        public PimplPtr()
        {
            ptr = IntPtr.Zero;
            deleter = IntPtr.Zero;
        }

        public PimplPtr(IntPtr ptr, IntPtr deleter)
        {
            this.ptr = ptr;
            this.deleter = deleter;
        }
    }
}