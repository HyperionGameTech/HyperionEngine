using System;

namespace Hyperion
{
    public interface IComponent : IDisposable
    {
        public static abstract Class Class { get; }
        public abstract IntPtr NativeAddress { get; }
    }
}