using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Vec2u
    {
        [FieldOffset(0)]
        public uint x;
        [FieldOffset(4)]
        public uint y;

        public Vec2u()
        {
            this.x = 0;
            this.y = 0;
        }

        public Vec2u(uint x, uint y)
        {
            this.x = x;
            this.y = y;
        }

        public Vec2u(uint value)
        {
            this.x = value;
            this.y = value;
        }

        public Vec2u(Vec2u other)
        {
            this.x = other.x;
            this.y = other.y;
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

        public static Vec2u operator+(Vec2u left, Vec2u right)
        {
            return new Vec2u(left.x + right.x, left.y + right.y);
        }

        public static Vec2u operator-(Vec2u left, Vec2u right)
        {
            return new Vec2u(left.x - right.x, left.y - right.y);
        }

        public static Vec2u operator*(Vec2u left, Vec2u right)
        {
            return new Vec2u(left.x * right.x, left.y * right.y);
        }

        public static Vec2u operator*(Vec2u left, uint right)
        {
            return new Vec2u(left.x * right, left.y * right);
        }

        public static Vec2u operator/(Vec2u left, Vec2u right)
        {
            return new Vec2u(left.x / right.x, left.y / right.y);
        }

        public static Vec2u operator/(Vec2u left, uint right)
        {
            return new Vec2u(left.x / right, left.y / right);
        }

        // @TODO: Remaining methods

        public override string ToString()
        {
            return $"[{x}, {y}]";
        }
    }
}