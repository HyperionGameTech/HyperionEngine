using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EnvProbeType")]
    public enum EnvProbeType : uint
    {
        Invalid = ~0u,
        Reflection = 0,
        Sky = 1,
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
}