using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WGObject")]
    [StructLayout(LayoutKind.Sequential)]
    public struct WGObject
    {
        public Vec2i coord;
        public AssetPath name;

        public WGObject()
        {
        }

        public WGObject(Vec2i coord, AssetPath name)
        {
            this.coord = coord;
            this.name = name;
        }
    }

    [ClassBinding(Name = "StreamableBase")]
    public abstract class StreamableBase : ObjectBase
    {
        public StreamableBase() : base()
        {
        }

        public abstract BoundingBox GetBoundingBox();

        public virtual void OnStreamStart()
        {
        }

        public virtual void OnLoaded()
        {
        }

        public virtual void OnRemoved()
        {
        }
    }
}