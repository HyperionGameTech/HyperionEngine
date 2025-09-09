using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [HypClassBinding(Name="BoundingSphere")]
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct BoundingSphere
    {
        [FieldOffset(0)]
        private Vec3f center;

        [FieldOffset(16)]
        private float radius;

        public BoundingSphere()
        {
            center = new Vec3f(0, 0, 0);
            radius = 0;
        }

        public BoundingSphere(Vec3f center, float radius)
        {
            this.center = center;
            this.radius = radius;
        }

        public Vec3f Center
        {
            get
            {
                return center;
            }
            set
            {
                center = value;
            }
        }

        public float Radius
        {
            get
            {
                return radius;
            }
            set
            {
                radius = value;
            }
        }
    }
}