using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8, Pack = 8)]
    public struct Vec2i
    {
        [FieldOffset(0)]
        public int x;
        [FieldOffset(4)]
        public int y;

        public Vec2i()
        {
            this.x = 0;
            this.y = 0;
        }

        public Vec2i(int x, int y)
        {
            this.x = x;
            this.y = y;
        }

        public Vec2i(int value)
        {
            this.x = value;
            this.y = value;
        }

        public Vec2i(Vec2i other)
        {
            this.x = other.x;
            this.y = other.y;
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

        public static Vec2i operator+(Vec2i left, Vec2i right)
        {
            return new Vec2i(left.x + right.x, left.y + right.y);
        }

        public static Vec2i operator-(Vec2i left, Vec2i right)
        {
            return new Vec2i(left.x - right.x, left.y - right.y);
        }

        public static Vec2i operator*(Vec2i left, Vec2i right)
        {
            return new Vec2i(left.x * right.x, left.y * right.y);
        }

        public static Vec2i operator*(Vec2i left, int right)
        {
            return new Vec2i(left.x * right, left.y * right);
        }

        public static Vec2i operator/(Vec2i left, Vec2i right)
        {
            return new Vec2i(left.x / right.x, left.y / right.y);
        }

        public static Vec2i operator/(Vec2i left, int right)
        {
            return new Vec2i(left.x / right, left.y / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}]";
        }
    }
}
