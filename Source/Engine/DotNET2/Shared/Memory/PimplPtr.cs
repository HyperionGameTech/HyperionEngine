using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// Wrapper struct corresponding to /core/memory/Pimpl.hpp in the C++ code
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct PimplPtr
    {
        [FieldOffset(0)]
        internal IntPtr ptr;
    }
}