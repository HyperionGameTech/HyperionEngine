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

        public bool IsValid
        {
            get
            {
                return min.x <= max.x && min.y <= max.y && min.z <= max.z;
            }
        }

        public bool IsFinite
        {
            get
            {
                return min.IsFinite && max.IsFinite;
            }
        }

        public bool IsEmpty
        {
            get
            {
                return min.x == float.MaxValue && min.y == float.MaxValue && min.z == float.MaxValue
                    && max.x == float.MinValue && max.y == float.MinValue && max.z == float.MinValue;
            }
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
                return BoundingBox_GetRadius(ref this);
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

        public bool Intersects(BoundingBox other)
        {
            return BoundingBox_Intersects(ref this, ref other);
        }

        public bool Contains(BoundingBox other)
        {
            return BoundingBox_Contains(ref this, ref other);
        }

        public bool ContainsPoint(Vec3f point)
        {
            return BoundingBox_ContainsPoint(ref this, ref point);
        }

        public static BoundingBox operator*(BoundingBox left, float right)
        {
            return new BoundingBox(left.min * right, left.max * right);
        }

        public static BoundingBox operator/(BoundingBox left, float right)
        {
            if (!left.IsValid)
            {
                return left;
            }

            return new BoundingBox(left.min / right, left.max / right);
        }

        public static BoundingBox operator+(BoundingBox left, Vec3f right)
        {
            return new BoundingBox(left.min + right, left.max + right);
        }

        public static BoundingBox operator/(BoundingBox left, Vec3f right)
        {
            return new BoundingBox(left.min / right, left.max / right);
        }

        public static BoundingBox operator*(BoundingBox left, Vec3f right)
        {
            return new BoundingBox(left.min * right, left.max * right);
        }

        [DllImport("hyperion", EntryPoint = "BoundingBox_GetRadius")]
        private static extern float BoundingBox_GetRadius([In] ref BoundingBox box);

        [DllImport("hyperion", EntryPoint = "BoundingBox_Intersects")]
        private static extern bool BoundingBox_Intersects([In] ref BoundingBox box, [In] ref BoundingBox other);

        [DllImport("hyperion", EntryPoint = "BoundingBox_Contains")]
        private static extern bool BoundingBox_Contains([In] ref BoundingBox box, [In] ref BoundingBox other);

        [DllImport("hyperion", EntryPoint = "BoundingBox_ContainsPoint")]
        private static extern bool BoundingBox_ContainsPoint([In] ref BoundingBox box, [In] ref Vec3f point);
    }
}