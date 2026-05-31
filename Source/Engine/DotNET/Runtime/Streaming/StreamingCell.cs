using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "StreamingCellState")]
    public enum StreamingCellState : uint
    {
        Unloaded = 0,
        Unloading,
        Waiting,
        Loaded
    }

    [ClassBinding(Name = "StreamingCellNeighbor")]
    [StructLayout(LayoutKind.Explicit, Size = 8, Pack = 8)]
    public struct StreamingCellNeighbor
    {
        [FieldOffset(0)]
        public Vec2i Coord;

        public Vec2f Center
        {
            get => new Vec2f(Coord.X, Coord.Y) - new Vec2f(0.5f, 0.5f);
        }
    }

    [ClassBinding(Name = "StreamingCellInfo")]
    [StructLayout(LayoutKind.Explicit, Size = 80, Pack = 16)]
    public struct StreamingCellInfo
    {
        [FieldOffset(0)]
        public Vec2i Coord;

        [FieldOffset(16)]
        public Vec3i Extent;

        [FieldOffset(32)]
        public Vec3f Scale;

        [FieldOffset(48)]
        public BoundingBox Bounds;
    }

    [ClassBinding(Name = "StreamingCell")]
    public class StreamingCell : StreamableBase
    {
        public StreamingCell()
        {
        }

        public override BoundingBox GetBoundingBox()
        {
            return InvokeNativeMethod<BoundingBox>(new Name("GetBoundingBox_Impl", weak: true));
        }

        public override void OnStreamStart()
        {
            InvokeNativeMethod(new Name("OnStreamStart_Impl", weak: true));
        }

        public override void OnLoaded()
        {
            InvokeNativeMethod(new Name("OnLoaded_Impl", weak: true));
        }

        public override void OnRemoved()
        {
            InvokeNativeMethod(new Name("OnRemoved_Impl", weak: true));
        }

        public virtual void Update(float delta)
        {
            InvokeNativeMethod(new Name("Update_Impl", weak: true), [delta]);
        }
    }
}