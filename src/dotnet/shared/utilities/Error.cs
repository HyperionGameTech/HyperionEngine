using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Error")]
    [StructLayout(LayoutKind.Explicit, Size = 128)]
    public struct Error
    {
        [FieldOffset(0)]
        private unsafe fixed byte data[128];
    }
}