using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum ShadowMapFilter : uint
    {
        None = 0x0,
        PCF = 0x1,
        CONTACT_HARDENED = 0x2,
        VSM = 0x3
    }

    [HypClassBinding(Name="ShadowMapComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct ShadowMapComponent : IComponent
    {
        [FieldOffset(0)]
        public ShadowMapFilter filter = ShadowMapFilter.PCF;
        [FieldOffset(4)]
        public float radius = 20.0f;
        [FieldOffset(8)]
        public Vec2u resolution = new Vec2u(512, 512);
        [FieldOffset(16)]
        public RefCountedPtr renderer = RefCountedPtr.Null;
        [FieldOffset(20)]
        private uint updateCounter = 0;

        public ShadowMapComponent()
        {
        }
    }
}
