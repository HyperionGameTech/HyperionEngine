using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightmapVolumeId : uint
    { }

    [ClassBinding(Name = "LightmapVolume")]
    public class LightmapVolume : VolumeBase
    {
        public LightmapVolume()
        {
        }
    }
}