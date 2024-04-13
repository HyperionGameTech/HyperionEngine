using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct BoundingBox
    {
        [FieldOffset(0)]
        private Vec3f min;
        [FieldOffset(16)]
        private Vec3f max;

        public BoundingBox(Vec3f min, Vec3f max)
        {
            this.min = min;
            this.max = max;
        }

        public Vec3f Min
        {
            get
            {
                return min;
            }
            set
            {
                min = value;
            }
        }

        public Vec3f Max
        {
            get
            {
                return max;
            }
            set
            {
                max = value;
            }
        }

        public Vec3f Extent
        {
            get
            {
                return max - min;
            }
            set
            {
                var center = Center;

                max = center + value * 0.5f;
                min = center - value * 0.5f;
            }
        }

        public Vec3f Center
        {
            get
            {
                return (min + max) * 0.5f;
            }
            set
            {
                var extent = Extent;

                max = value + extent * 0.5f;
                min = value - extent * 0.5f;
            }
        }

        public float Radius
        {
            get
            {
                return BoundingBox_GetRadius(this);
            }
        }

        public Vec3f[] Corners
        {
            get
            {
                return new Vec3f[] {
                    new Vec3f(min.x, min.y, min.z),
                    new Vec3f(max.x, min.y, min.z),
                    new Vec3f(max.x, max.y, min.z),
                    new Vec3f(min.x, max.y, min.z),
                    new Vec3f(min.x, min.y, max.z),
                    new Vec3f(min.x, max.y, max.z),
                    new Vec3f(max.x, max.y, max.z),
                    new Vec3f(max.x, min.y, max.z)
                };
            }
        }

        public bool Contains(BoundingBox other)
        {
            return BoundingBox_Contains(this, other);
        }

        public bool ContainsPoint(Vec3f point)
        {
            return BoundingBox_ContainsPoint(this, point);
        }

        [DllImport("hyperion", EntryPoint = "BoundingBox_GetRadius")]
        private static extern float BoundingBox_GetRadius(BoundingBox box);

        [DllImport("hyperion", EntryPoint = "BoundingBox_Contains")]
        public static extern bool BoundingBox_Contains(BoundingBox box, BoundingBox other);

        [DllImport("hyperion", EntryPoint = "BoundingBox_ContainsPoint")]
        public static extern bool BoundingBox_ContainsPoint(BoundingBox box, Vec3f point);
    }
}