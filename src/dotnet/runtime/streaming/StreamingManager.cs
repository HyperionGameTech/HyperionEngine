using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "StreamingVolumeShape")]
    public enum StreamingVolumeShape : uint
    {
        Sphere = 0,
        Box = 1,

        Max,

        Invalid = ~0u
    }

    [HypClassBinding(Name = "StreamingVolumeBase")]
    public abstract class StreamingVolumeBase : HypObject
    {
        public StreamingVolumeBase() : base()
        {
        }

        public abstract StreamingVolumeShape GetShape();
        public abstract bool GetBoundingBox(ref BoundingBox boundingBox);
        public abstract bool GetBoundingSphere(ref BoundingSphere boundingSphere);
        public abstract bool ContainsPoint(Vec3f point);
    }

    [HypClassBinding(Name = "StreamingManager")]
    public class StreamingManager : HypObject
    {
        public StreamingManager() : base()
        {
        }
    }
}