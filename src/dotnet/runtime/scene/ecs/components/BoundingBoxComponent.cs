using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 64)]
    public struct BoundingBoxComponent : IComponent
    {
        [FieldOffset(0)]
        public BoundingBox localAabb;
        [FieldOffset(32)]
        public BoundingBox worldAabb;
    }
}