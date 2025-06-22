using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EnvProbeType")]
    public enum EnvProbeType : uint
    {
        Invalid = ~0u,
        Sky = 0,
        Reflection = 1,
        Shadow = 2,
        Ambient = 3
    }

    [HypClassBinding(Name="EnvProbe")]
    public class EnvProbe : Entity
    {
        public EnvProbe()
        {
        }
    }

    [HypClassBinding(Name="ReflectionProbe")]
    public class ReflectionProbe : EnvProbe
    {
        public ReflectionProbe()
        {
        }
    }

    [HypClassBinding(Name="SkyProbe")]
    public class SkyProbe : EnvProbe
    {
        public SkyProbe()
        {
        }
    }
}