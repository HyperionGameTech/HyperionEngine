using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Vec2f")]
    [StructLayout(LayoutKind.Explicit, Size = 8, Pack = 8)]
    public struct Vec2f
    {
        [FieldOffset(0)]
        public float x;
        [FieldOffset(4)]
        public float y;

        public Vec2f()
        {
            this.x = 0;
            this.y = 0;
        }

        public Vec2f(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public Vec2f(float value)
        {
            this.x = value;
            this.y = value;
        }

        public Vec2f(Vec2f other)
        {
            this.x = other.x;
            this.y = other.y;
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

        public float Length()
        {
            return MathF.Sqrt(x * x + y * y);
        }

        public float LengthSquared()
        {
            return x * x + y * y;
        }

        public Vec2f Normalize()
        {
            float length = Length();

            if (length == 0)
            {
                return new Vec2f(0, 0);
            }

            return new Vec2f(x / length, y / length);
        }

        public static Vec2f operator+(Vec2f left, Vec2f right)
        {
            return new Vec2f(left.x + right.x, left.y + right.y);
        }

        public static Vec2f operator-(Vec2f left, Vec2f right)
        {
            return new Vec2f(left.x - right.x, left.y - right.y);
        }

        public static Vec2f operator*(Vec2f left, Vec2f right)
        {
            return new Vec2f(left.x * right.x, left.y * right.y);
        }

        public static Vec2f operator*(Vec2f left, float right)
        {
            return new Vec2f(left.x * right, left.y * right);
        }

        public static Vec2f operator/(Vec2f left, Vec2f right)
        {
            return new Vec2f(left.x / right.x, left.y / right.y);
        }

        public static Vec2f operator/(Vec2f left, float right)
        {
            return new Vec2f(left.x / right, left.y / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}]";
        }
    }
}