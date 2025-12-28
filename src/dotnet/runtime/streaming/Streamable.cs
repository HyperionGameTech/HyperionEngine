using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WGObject")]
    [StructLayout(LayoutKind.Sequential)]
    public struct WGObject
    {
        public Vec2i Coord;
        public AssetPath Name;

        public WGObject()
        {
        }

        public WGObject(Vec2i coord, AssetPath name)
        {
            Coord = coord;
            Name = name;
        }
    }

    [ClassBinding(Name = "StreamableBase")]
    public class StreamableBase : ObjectBase
    {
        public StreamableBase() : base()
        {
        }

        public virtual BoundingBox GetBoundingBox()
        {
            return InvokeNativeMethod<BoundingBox>(new Name("GetBoundingBox_Impl", weak: true));
        }

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