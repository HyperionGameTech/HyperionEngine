using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct Vec3u
    {
        [FieldOffset(0)]
        public uint x;
        [FieldOffset(4)]
        public uint y;
        [FieldOffset(8)]
        public uint z;

        public Vec3u()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
        }

        public Vec3u(uint x, uint y, uint z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vec3u(uint value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
        }

        public Vec3u(Vec3u other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
        }

        uint this[int index]
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

        public uint X
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

        public uint Y
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

        public uint Z
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

        public static Vec3u operator+(Vec3u left, Vec3u right)
        {
            return new Vec3u(left.x + right.x, left.y + right.y, left.z + right.z);
        }

        public static Vec3u operator-(Vec3u left, Vec3u right)
        {
            return new Vec3u(left.x - right.x, left.y - right.y, left.z - right.z);
        }

        public static Vec3u operator*(Vec3u left, Vec3u right)
        {
            return new Vec3u(left.x * right.x, left.y * right.y, left.z * right.z);
        }

        public static Vec3u operator*(Vec3u left, uint right)
        {
            return new Vec3u(left.x * right, left.y * right, left.z * right);
        }

        public static Vec3u operator/(Vec3u left, Vec3u right)
        {
            return new Vec3u(left.x / right.x, left.y / right.y, left.z / right.z);
        }

        public static Vec3u operator/(Vec3u left, uint right)
        {
            return new Vec3u(left.x / right, left.y / right, left.z / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}]";
        }
    }
}