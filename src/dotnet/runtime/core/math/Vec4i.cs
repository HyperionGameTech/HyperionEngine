using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Vec4i")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Vec4i
    {
        [FieldOffset(0)]
        public int x;
        [FieldOffset(4)]
        public int y;
        [FieldOffset(8)]
        public int z;
        [FieldOffset(12)]
        public int w;

        public Vec4i()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
            this.w = 0;
        }

        public Vec4i(int x, int y, int z, int w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public Vec4i(int value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
            this.w = value;
        }

        public Vec4i(Vec4i other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
            this.w = other.w;
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

        public int W
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

        public static Vec4i operator+(Vec4i left, Vec4i right)
        {
            return new Vec4i(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
        }

        public static Vec4i operator-(Vec4i left, Vec4i right)
        {
            return new Vec4i(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
        }

        public static Vec4i operator*(Vec4i left, Vec4i right)
        {
            return new Vec4i(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
        }

        public static Vec4i operator*(Vec4i left, int right)
        {
            return new Vec4i(left.x * right, left.y * right, left.z * right, left.w * right);
        }

        public static Vec4i operator/(Vec4i left, Vec4i right)
        {
            return new Vec4i(left.x / right.x, left.y / right.y, left.z / right.z, left.w / right.w);
        }

        public static Vec4i operator/(Vec4i left, int right)
        {
            return new Vec4i(left.x / right, left.y / right, left.z / right, left.w / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}, {w}]";
        }
    }
}