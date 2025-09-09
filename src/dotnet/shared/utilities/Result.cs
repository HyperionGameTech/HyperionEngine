using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Maps to core/utilities/Result.hpp
    [HypClassBinding(Name="Result")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Result
    {
        [FieldOffset(0)]
        private PimplPtr errorPtr;

        public bool IsValid
        {
            get
            {
                return errorPtr.ptr == IntPtr.Zero;
            }
        }
    }
}