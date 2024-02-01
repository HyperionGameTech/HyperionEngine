using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct Vec3f
    {
        [FieldOffset(0)]
        public float x;
        [FieldOffset(4)]
        public float y;
        [FieldOffset(8)]
        public float z;

        public Vec3f()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
        }

        public Vec3f(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vec3f(float value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
        }

        public Vec3f(Vec3f other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
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

        public static Vec3f operator+(Vec3f left, Vec3f right)
        {
            return new Vec3f(left.x + right.x, left.y + right.y, left.z + right.z);
        }

        public static Vec3f operator-(Vec3f left, Vec3f right)
        {
            return new Vec3f(left.x - right.x, left.y - right.y, left.z - right.z);
        }

        public static Vec3f operator*(Vec3f left, Vec3f right)
        {
            return new Vec3f(left.x * right.x, left.y * right.y, left.z * right.z);
        }

        public static Vec3f operator*(Vec3f left, float right)
        {
            return new Vec3f(left.x * right, left.y * right, left.z * right);
        }

        public static Vec3f operator/(Vec3f left, Vec3f right)
        {
            return new Vec3f(left.x / right.x, left.y / right.y, left.z / right.z);
        }

        public static Vec3f operator/(Vec3f left, float right)
        {
            return new Vec3f(left.x / right, left.y / right, left.z / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}, {z}]";
        }
    }
}