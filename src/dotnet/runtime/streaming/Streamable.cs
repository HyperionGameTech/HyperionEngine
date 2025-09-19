using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "StreamableKey")]
    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct StreamableKey
    {
        [FieldOffset(0)]
        public Uuid uuid;

        [FieldOffset(16)]
        public Name name;

        public StreamableKey()
        {
            this.uuid = Uuid.Invalid;
            this.name = Name.Invalid;
        }

        public StreamableKey(Uuid uuid, Name name)
        {
            this.uuid = uuid;
            this.name = name;
        }
    }

    [HypClassBinding(Name = "StreamableBase")]
    public abstract class StreamableBase : HypObject
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