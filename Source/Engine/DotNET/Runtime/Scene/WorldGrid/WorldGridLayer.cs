using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WorldGridLayerInfo")]
    [StructLayout(LayoutKind.Sequential)]
    public struct WorldGridLayerInfo
    {
        public Vec3f offset;
        public Vec3f scale;
        public Vec2i range; // only relevant if infinite==false
        public uint cellSize;
        public float maxDistance;
        public uint seed;

        [MarshalAs(UnmanagedType.I1)]
        public bool infinite;

        public WorldGridLayerInfo()
        {
            offset = new Vec3f(0.0f, 0.0f, 0.0f);
            scale = new Vec3f(1.0f, 1.0f, 1.0f);
            range = new Vec2i(-10, 10);
            cellSize = 32;
            maxDistance = 1.0f;
            seed = 0;
            infinite = true;
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