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
        public Vec2i coord;

        public Vec2f Center
        {
            get
            {
                return new Vec2f((float)coord.X, (float)coord.Y) - new Vec2f(0.5f, 0.5f);
            }
        }
    }

    [ClassBinding(Name = "StreamingCellInfo")]
    [StructLayout(LayoutKind.Explicit, Size = 80, Pack = 16)]
    public struct StreamingCellInfo
    {
        [FieldOffset(0)]
        Vec2i coord;

        [FieldOffset(16)]
        Vec3i extent;

        [FieldOffset(32)]
        Vec3f scale;

        [FieldOffset(48)]
        BoundingBox bounds;
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
            Console.WriteLine("StreamingCell.OnStreamStart() called");
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
            InvokeNativeMethod(new Name("Update_Impl", weak: true), new object[] { delta });
        }
    }
}