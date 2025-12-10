using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Maps to core/utilities/Result.hpp
    [ClassBinding(Name="Result")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Result
    {
        [FieldOffset(0)]
        private PimplPtr _error;

        public bool IsValid
        {
            get
            {
                return _error.ptr == IntPtr.Zero;
            }
        }
    }
}