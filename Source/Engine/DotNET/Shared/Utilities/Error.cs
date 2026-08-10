using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Error")]
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct Error
    {
        [FieldOffset(0)]
        private unsafe byte* _message;

        [FieldOffset(8)]
        private unsafe byte* _currentFunction;

        public Error()
        {
            unsafe
            {
                _message = null;
                _currentFunction = null;
            }
        }
    }
}