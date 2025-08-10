using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "LightmapTextureType")]
    public enum LightmapTextureType : uint
    {
        Invalid = uint.MaxValue,

        Radiance = 0,
        Irradiance,

        Count
    }

    [HypClassBinding(Name = "LightmapVolume")]
    public class LightmapVolume : Entity
    {
        public LightmapVolume()
        {
        }
    }
}