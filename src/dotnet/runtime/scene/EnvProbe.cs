using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="EnvProbeType")]
    public enum EnvProbeType : uint
    {
        Invalid = ~0u,
        Sky = 0,
        Reflection = 1,
        Shadow = 2,
        Ambient = 3
    }

    [ClassBinding(Name="EnvProbe")]
    public class EnvProbe : Entity
    {
        public EnvProbe()
        {
        }
    }

    [ClassBinding(Name="ReflectionProbe")]
    public class ReflectionProbe : EnvProbe
    {
        public ReflectionProbe()
        {
        }
    }

    [ClassBinding(Name="SkyProbe")]
    public class SkyProbe : EnvProbe
    {
        public SkyProbe()
        {
        }
    }
}