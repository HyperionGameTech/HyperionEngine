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
    public abstract class StreamableBase : ObjectBase
    {
        public StreamableBase() : base()
        {
        }
    }
}