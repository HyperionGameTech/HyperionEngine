using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightComponentFlags : uint
    {
        None = 0x0,
        Init = 0x1
    }

    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct LightComponent : IComponent
    {
        [FieldOffset(0)]
        private ManagedHandle lightHandle;
        [FieldOffset(8)]
        private HashCode transformHashCode;
        [FieldOffset(16)]
        private LightComponentFlags lightComponentFlags;

        public Light Light
        {
            get
            {
                return new Light(lightHandle);
            }
            set
            {
                lightHandle = value.Handle;
            }
        }
    }
}