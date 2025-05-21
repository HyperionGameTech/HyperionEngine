using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="MeshInstanceData")]
    [StructLayout(LayoutKind.Sequential, Size = 104, Pack = 8)]
    public struct MeshInstanceData
    {
        private unsafe fixed byte arrayData[104];
    }
}