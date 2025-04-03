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
    [StructLayout(LayoutKind.Explicit, Size = 40)]
    public struct ShadowMapComponent : IComponent
    {
        [FieldOffset(0)]
        private ShadowMapFilter filter = ShadowMapFilter.PCF;
        [FieldOffset(4)]
        private float radius = 20.0f;
        [FieldOffset(8)]
        private Vec2u resolution = new Vec2u(512, 512);
        [FieldOffset(16)]
        private RefCountedPtr renderer;
        [FieldOffset(24)]
        private uint updateCounter = 0;

        public ShadowMapComponent()
        {
        }

        public ShadowMapFilter Filter
        {
            get
            {
                return filter;
            }
            set
            {
                filter = value;
            }
        }

        public float Radius
        {
            get
            {
                return radius;
            }
            set
            {
                radius = value;
            }
        }

        public Vec2u Resolution
        {
            get
            {
                return resolution;
            }
            set
            {
                resolution = value;
            }
        }

        public uint UpdateCounter
        {
            get
            {
                return updateCounter;
            }
        }
    }
}
