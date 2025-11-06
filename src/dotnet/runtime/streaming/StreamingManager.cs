using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "StreamingVolumeShape")]
    public enum StreamingVolumeShape : uint
    {
        Sphere = 0,
        Box = 1,

        Max,

        Invalid = ~0u
    }

    [ClassBinding(Name = "StreamingVolumeBase")]
    public abstract class StreamingVolumeBase : ObjectBase
    {
        public StreamingVolumeBase() : base()
        {
        }

        public abstract StreamingVolumeShape GetShape();
        public abstract bool GetBoundingBox(ref BoundingBox boundingBox);
        public abstract bool GetBoundingSphere(ref BoundingSphere boundingSphere);
        public abstract bool ContainsPoint(Vec3f point);
    }

    [ClassBinding(Name = "StreamingManager")]
    public class StreamingManager : ObjectBase
    {
        public StreamingManager() : base()
        {
        }
    }
}