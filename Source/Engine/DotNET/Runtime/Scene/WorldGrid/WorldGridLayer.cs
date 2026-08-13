using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WorldGridLayerInfo")]
    [StructLayout(LayoutKind.Explicit, Size = 48, Pack = 16)]
    public struct WorldGridLayerInfo
    {
        [FieldOffset(0)]
        public Vec3f offset;

        [FieldOffset(16)]
        public Vec3f scale;

        [FieldOffset(32)]
        public uint cellSize;

        [FieldOffset(36)]
        public float maxDistance;

        public WorldGridLayerInfo()
        {
            offset = new Vec3f(0.0f, 0.0f, 0.0f);
            scale = new Vec3f(1.0f, 1.0f, 1.0f);
            cellSize = 32;
            maxDistance = 1.0f;
        }
    }

    [ClassBinding(Name = "WorldGridLayer")]
    public class WorldGridLayer : ObjectBase
    {
        public WorldGridLayer() : base()
        {
        }
    }
}