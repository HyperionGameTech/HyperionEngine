using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Vec3f")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
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

        public bool IsFinite
        {
            get
            {
                return float.IsFinite(x) && float.IsFinite(y) && float.IsFinite(z);
            }
        }

        public float Length()
        {
            return MathF.Sqrt(LengthSquared());
        }

        public float LengthSquared()
        {
            return x * x + y * y + z * z;
        }

        public Vec3f Normalize()
        {
            float length = Length();

            if (length == 0)
            {
                return new Vec3f(0, 0, 0);
            }

            return new Vec3f(x / length, y / length, z / length);
        }

        public Vec3f Cross(Vec3f other)
        {
            return new Vec3f(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }

        public float Dot(Vec3f other)
        {
            return x * other.x + y * other.y + z * other.z;
        }

        public Vec3f Lerp(Vec3f other, float t)
        {
            return new Vec3f(
                x + (other.x - x) * t,
                y + (other.y - y) * t,
                z + (other.z - z) * t
            );
        }

        public float Distance(Vec3f other)
        {
            return (float)Math.Sqrt(DistanceSquared(other));
        }

        public float DistanceSquared(Vec3f other)
        {
            return (x - other.x) * (x - other.x) + (y - other.y) * (y - other.y) + (z - other.z) * (z - other.z);
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