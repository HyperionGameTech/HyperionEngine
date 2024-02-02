using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 72)]
    public struct BoundingBoxComponent : IComponent
    {
        [FieldOffset(0)]
        public BoundingBox localAabb;
        [FieldOffset(32)]
        public BoundingBox worldAabb;
        [FieldOffset(64)]
        private HashCode transformHashCode;
    }
}