using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightComponentFlags : uint
    {
        None = 0x0
    }

    [HypClassBinding(Name="LightComponent")]
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
                throw new NotImplementedException();
                // return new Light(lightHandle);
            }
            set
            {
                throw new NotImplementedException();
                // lightHandle = value.Handle;
            }
        }
    }
}