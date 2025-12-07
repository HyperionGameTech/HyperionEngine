using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="MeshInstanceData")]
    [StructLayout(LayoutKind.Explicit, Size = 104)]
    public struct MeshInstanceData
    {
        [FieldOffset(0)]
        private unsafe fixed byte _arrayData[104];
    }
}