using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Vec4u")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Vec4u
    {
        [FieldOffset(0)]
        public uint x;
        [FieldOffset(4)]
        public uint y;
        [FieldOffset(8)]
        public uint z;
        [FieldOffset(12)]
        public uint w;

        public Vec4u()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
            this.w = 0;
        }

        public Vec4u(uint x, uint y, uint z, uint w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public Vec4u(uint value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
            this.w = value;
        }

        public Vec4u(Vec4u other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
            this.w = other.w;
        }

        uint this[uint index]
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
                else if (index == 3)
                {
                    return w;
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
                else if (index == 3)
                {
                    w = value;
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

        public uint W
        {
            get
            {
                return w;
            }
            set
            {
                w = value;
            }
        }

        public static Vec4u operator+(Vec4u left, Vec4u right)
        {
            return new Vec4u(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
        }

        public static Vec4u operator-(Vec4u left, Vec4u right)
        {
            return new Vec4u(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
        }

        public static Vec4u operator*(Vec4u left, Vec4u right)
        {
            return new Vec4u(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
        }

        public static Vec4u operator*(Vec4u left, uint right)
        {
            return new Vec4u(left.x * right, left.y * right, left.z * right, left.w * right);
        }

        public static Vec4u operator/(Vec4u left, Vec4u right)
        {
            return new Vec4u(left.x / right.x, left.y / right.y, left.z / right.z, left.w / right.w);
        }

        public static Vec4u operator/(Vec4u left, uint right)
        {
            return new Vec4u(left.x / right, left.y / right, left.z / right, left.w / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}, {w}]";
        }
    }
}