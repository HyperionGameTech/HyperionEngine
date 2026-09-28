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
        Baked = 0x2,
        Realtime = 0x4,
        OriginFromCenter = 0x8,
        Visibility = 0x10,
        PathTraced = 0x20,
        HitMask = 0x40,
        OnlySameScene = 0x80
    }

    [ClassBinding(Name = "EnvProbeType")]
    public enum EnvProbeType : uint
    {
        Invalid = ~0u,
        Sky = 0,
        Reflection = 1,
        Ambient = 2
    }

    [ClassBinding(Name = "EnvProbeDimensions")]
    public enum EnvProbeDimensions : ushort
    {
        Dim64 = 64,
        Dim128 = 128,
        Dim256 = 256
    }

    [ClassBinding(Name = "EnvProbe")]
    public class EnvProbe : VolumeBase
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

    [ClassBinding(Name = "IrradianceProbe")]
    public class IrradianceProbe : EnvProbe
    {
        public IrradianceProbe()
        {
        }
    }
}
