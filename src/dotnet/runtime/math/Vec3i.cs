using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Vec3i
    {
        [FieldOffset(0)]
        public int x;
        [FieldOffset(4)]
        public int y;
        [FieldOffset(8)]
        public int z;

        public Vec3i()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
        }

        public Vec3i(int x, int y, int z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vec3i(int value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
        }

        public Vec3i(Vec3i other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
        }

        int this[int index]
        {
            get
            {
                if (index == 0)
                {
                    return x;
                }
                else if (index == 1)
                {
                    return y;
                }
                else if (index == 2)
                {
                    return z;
                }
                else
                {
                    throw new IndexOutOfRangeException();
                }
            }
            set
            {
                if (index == 0)
                {
                    x = value;
                }
                else if (index == 1)
                {
                    y = value;
                }
                else if (index == 2)
                {
                    z = value;
                }
                else
                {
                    throw new IndexOutOfRangeException();
                }
            }
        }

        public int X
        {
            get
            {
                return x;
            }
            set
            {
                x = value;
            }
        }

        public int Y
        {
            get
            {
                return y;
            }
            set
            {
                y = value;
            }
        }

        public int Z
        {
            get
            {
                return z;
            }
            set
            {
                z = value;
            }
        }

        public static Vec3i operator+(Vec3i left, Vec3i right)
        {
            return new Vec3i(left.x + right.x, left.y + right.y, left.z + right.z);
        }

        public static Vec3i operator-(Vec3i left, Vec3i right)
        {
            return new Vec3i(left.x - right.x, left.y - right.y, left.z - right.z);
        }

        public static Vec3i operator*(Vec3i left, Vec3i right)
        {
            return new Vec3i(left.x * right.x, left.y * right.y, left.z * right.z);
        }

        public static Vec3i operator*(Vec3i left, int right)
        {
            return new Vec3i(left.x * right, left.y * right, left.z * right);
        }

        public static Vec3i operator/(Vec3i left, Vec3i right)
        {
            return new Vec3i(left.x / right.x, left.y / right.y, left.z / right.z);
        }

        public static Vec3i operator/(Vec3i left, int right)
        {
            return new Vec3i(left.x / right, left.y / right, left.z / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}]";
        }
    }
}
