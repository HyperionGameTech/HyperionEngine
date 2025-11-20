using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EnvProbeFlags")]
    [Flags]
    public enum EnvProbeFlags : uint
    {
        None = 0x0,
        ParallaxCorrected = 0x1,
        Baked = 0x2
    }

    [ClassBinding(Name = "EnvProbeType")]
    public enum EnvProbeType : uint
    {
        Invalid = ~0u,
        Sky = 0,
        Reflection = 1,
        Ambient = 3
    }

    [ClassBinding(Name = "EnvProbe")]
    public class EnvProbe : Entity
    {
        public EnvProbe()
        {
        }
    }

    [ClassBinding(Name = "ReflectionProbe")]
    public class ReflectionProbe : EnvProbe
    {
        public ReflectionProbe()
        {
        }
    }

    [ClassBinding(Name = "SkyProbe")]
    public class SkyProbe : EnvProbe
    {
        public SkyProbe()
        {
        }
    }
}