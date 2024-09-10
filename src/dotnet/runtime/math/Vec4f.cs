using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Vec4f")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Vec4f
    {
        [FieldOffset(0)]
        public float x;
        [FieldOffset(4)]
        public float y;
        [FieldOffset(8)]
        public float z;
        [FieldOffset(12)]
        public float w;

        public Vec4f()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
            this.w = 0;
        }

        public Vec4f(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public Vec4f(float value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
            this.w = value;
        }

        public Vec4f(Vec4f other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
            this.w = other.w;
        }

        float this[int index]
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

        public float X
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

        public float Y
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

        public float Z
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

        public float W
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

        public bool IsFinite
        {
            get
            {
                return float.IsFinite(x) && float.IsFinite(y) && float.IsFinite(z) && float.IsFinite(w);
            }
        }

        public static Vec4f operator+(Vec4f left, Vec4f right)
        {
            return new Vec4f(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
        }

        public static Vec4f operator-(Vec4f left, Vec4f right)
        {
            return new Vec4f(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
        }

        public static Vec4f operator*(Vec4f left, Vec4f right)
        {
            return new Vec4f(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
        }

        public static Vec4f operator*(Vec4f left, float right)
        {
            return new Vec4f(left.x * right, left.y * right, left.z * right, left.w * right);
        }

        public static Vec4f operator/(Vec4f left, Vec4f right)
        {
            return new Vec4f(left.x / right.x, left.y / right.y, left.z / right.z, left.w / right.w);
        }

        public static Vec4f operator/(Vec4f left, float right)
        {
            return new Vec4f(left.x / right, left.y / right, left.z / right, left.w / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}, {w}]";
        }
    }
}