using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "WorldGridLayerInfo")]
    [StructLayout(LayoutKind.Explicit, Size = 80, Pack = 16)]
    public struct WorldGridLayerInfo
    {
        [FieldOffset(0)]
        public Vec2u gridSize;

        [FieldOffset(16)]
        public Vec3u cellSize;

        [FieldOffset(32)]
        public Vec3f offset;

        [FieldOffset(48)]
        public Vec3f scale;

        [FieldOffset(64)]
        public float maxDistance;

        public WorldGridLayerInfo()
        {
            gridSize = new Vec2u(64, 64);
            cellSize = new Vec3u(32, 32, 32);
            offset = new Vec3f(0.0f, 0.0f, 0.0f);
            scale = new Vec3f(1.0f, 1.0f, 1.0f);
            maxDistance = 1.0f;
        }
    }

    [HypClassBinding(Name = "WorldGridLayer")]
    public class WorldGridLayer : HypObject
    {
        public WorldGridLayer() : base()
        {
        }

        public virtual void Init()
        {
            InvokeNativeMethod(new Name("Init_Impl", weak: true));
        }

        public virtual void OnAdded(WorldGrid worldGrid)
        {
            InvokeNativeMethod(new Name("OnAdded_Impl", weak: true), new object[] { worldGrid });
        }

        public virtual void OnRemoved(WorldGrid worldGrid)
        {
            InvokeNativeMethod(new Name("OnRemoved_Impl", weak: true), new object[] { worldGrid });
        }

        public virtual StreamingCell CreateStreamingCell(StreamingCellInfo cellInfo)
        {
            return InvokeNativeMethod<StreamingCell>(new Name("CreateStreamingCell_Impl", weak: true), new object[] { cellInfo });
        }

        public virtual WorldGridLayerInfo CreateLayerInfo()
        {
            return InvokeNativeMethod<WorldGridLayerInfo>(new Name("CreateLayerInfo_Impl", weak: true));
        }
    }
}