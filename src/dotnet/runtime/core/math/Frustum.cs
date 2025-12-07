using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Frustum")]
    [StructLayout(LayoutKind.Explicit, Size = 224, Pack = 16)]
    public unsafe struct Frustum
    {
        [FieldOffset(0)]
        private unsafe fixed float _planes[6 * 4];

        [FieldOffset(96)]
        private unsafe fixed float _corners[8 * 4];

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
                _planes[i * 4 + 0] = planes[i].x;
                _planes[i * 4 + 1] = planes[i].y;
                _planes[i * 4 + 2] = planes[i].z;
                _planes[i * 4 + 3] = planes[i].w;
            }

            for (int i = 0; i < 8; i++)
            {
                _corners[i * 4 + 0] = corners[i].x;
                _corners[i * 4 + 1] = corners[i].y;
                _corners[i * 4 + 2] = corners[i].z;
                _corners[i * 4 + 3] = 1.0f;
            }
        }

        public Vec4f GetPlane(int index)
        {
            if (index < 0 || index >= 6)
            {
                throw new ArgumentOutOfRangeException("index");
            }

            return new Vec4f(_planes[index * 4 + 0], _planes[index * 4 + 1], _planes[index * 4 + 2], _planes[index * 4 + 3]);
        }

        public Vec3f GetCorner(int index)
        {
            if (index < 0 || index >= 8)
            {
                throw new ArgumentOutOfRangeException("index");
            }

            return new Vec3f(_corners[index * 4 + 0], _corners[index * 4 + 1], _corners[index * 4 + 2]);
        }
    }
}