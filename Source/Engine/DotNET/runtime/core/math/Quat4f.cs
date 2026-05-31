using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Quat4f")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 16)]
    public struct Quat4f
    {
        public static readonly Quat4f Identity = new Quat4f(0.0f, 0.0f, 0.0f, 1.0f);

        [FieldOffset(0)]
        private float x;
        [FieldOffset(4)]
        private float y;
        [FieldOffset(8)]
        private float z;
        [FieldOffset(12)]
        private float w;

        public Quat4f()
        {
            this.x = 0;
            this.y = 0;
            this.z = 0;
            this.w = 1;
        }

        public Quat4f(float x, float y, float z, float w)
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