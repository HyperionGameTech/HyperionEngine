using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "LightmapTextureType")]
    public enum LightmapTextureType : uint
    {
        Invalid = uint.MaxValue,

        Radiance = 0,
        Irradiance,

        Count
    }

    [ClassBinding(Name = "LightmapVolume")]
    public class LightmapVolume : ObjectBase
    {
        public LightmapVolume()
        {
        }
    }
}