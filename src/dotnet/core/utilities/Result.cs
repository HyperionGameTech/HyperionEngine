using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Maps to core/utilities/Result.hpp
    [HypClassBinding(Name="Result")]
    [StructLayout(LayoutKind.Explicit, Size = 136)]
    public struct Result
    {
        [FieldOffset(0)]
        private Error error;

        [FieldOffset(128)]
        private bool hasError;
        
        public Error? Error
        {
            get
            {
                return hasError ? (Error?)error : null;
            }
        }

        public bool HasError
        {
            get
            {
                return hasError;
            }
        }

        public bool IsValid
        {
            get
            {
                return !hasError;
            }
        }
    }
}