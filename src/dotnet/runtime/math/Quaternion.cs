using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Quaternion
    {
        [FieldOffset(0)]
        private float x;
        [FieldOffset(4)]
        private float y;
        [FieldOffset(8)]
        private float z;
        [FieldOffset(12)]
        private float w;

        public Quaternion()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
            this.w = 1;
        }

        public Quaternion(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
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

        public override string ToString()
        {
            return $"[{x}, {y}, {z}, {w}]";
        }
    }
}