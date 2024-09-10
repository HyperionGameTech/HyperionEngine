using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Ray")]
    [StructLayout(LayoutKind.Explicit, Size = 32, Pack = 16)]
    public struct Ray
    {
        [FieldOffset(0)]
        private Vec3f position;

        [FieldOffset(16)]
        private Vec3f direction;

        public Ray()
        {
        }

        public Ray(Vec3f position, Vec3f direction)
        {
            this.position = position;
            this.direction = direction;
        }

        public Vec3f Position
        {
            get
            {
                return position;
            }
            set
            {
                position = value;
            }
        }

        public Vec3f Direction
        {
            get
            {
                return direction;
            }
            set
            {
                direction = value;
            }
        }
    }
}