using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Frustum")]
    [StructLayout(LayoutKind.Explicit, Size = 224, Pack = 16)]
    public unsafe struct Frustum
    {
        [FieldOffset(0)]
        private unsafe fixed float planes[6 * 4];

        [FieldOffset(96)]
        private unsafe fixed float corners[8 * 4];

        public Frustum()
        {
        }

        public Frustum(Vec4f[] planes, Vec3f[] corners)
        {
            if (planes.Length != 6)
            {
                throw new ArgumentException("planes must have a length of 6");
            }

            if (corners.Length != 8)
            {
                throw new ArgumentException("corners must have a length of 8");
            }

            for (int i = 0; i < 6; i++)
            {
                this.planes[i * 4 + 0] = planes[i].x;
                this.planes[i * 4 + 1] = planes[i].y;
                this.planes[i * 4 + 2] = planes[i].z;
                this.planes[i * 4 + 3] = planes[i].w;
            }

            for (int i = 0; i < 8; i++)
            {
                this.corners[i * 4 + 0] = corners[i].x;
                this.corners[i * 4 + 1] = corners[i].y;
                this.corners[i * 4 + 2] = corners[i].z;
                this.corners[i * 4 + 3] = 1.0f;
            }
        }

        public Vec4f GetPlane(int index)
        {
            if (index < 0 || index >= 6)
            {
                throw new ArgumentOutOfRangeException("index");
            }

            return new Vec4f(this.planes[index * 4 + 0], this.planes[index * 4 + 1], this.planes[index * 4 + 2], this.planes[index * 4 + 3]);
        }

        public Vec3f GetCorner(int index)
        {
            if (index < 0 || index >= 8)
            {
                throw new ArgumentOutOfRangeException("index");
            }

            return new Vec3f(this.corners[index * 4 + 0], this.corners[index * 4 + 1], this.corners[index * 4 + 2]);
        }
    }
}