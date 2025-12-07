using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [ClassBinding(Name="BoundingSphere")]
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct BoundingSphere
    {
        [FieldOffset(0)]
        public Vec3f Center;

        [FieldOffset(16)]
        public float Radius;

        public BoundingSphere()
        {
            Center = new Vec3f(0, 0, 0);
            Radius = 0;
        }

        public BoundingSphere(Vec3f center, float radius)
        {
            Center = center;
            Radius = radius;
        }
    }
}