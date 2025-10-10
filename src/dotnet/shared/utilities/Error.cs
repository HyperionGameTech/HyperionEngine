using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Error")]
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct Error
    {
        [FieldOffset(0)]
        private unsafe byte* message;

        [FieldOffset(8)]
        private unsafe byte* currentFunction;
    }
}