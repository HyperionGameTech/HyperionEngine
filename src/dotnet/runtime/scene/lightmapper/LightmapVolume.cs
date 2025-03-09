using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // See scene/lightmapper/LightmapVolume.hpp
    public interface LightmapElement
    {
        bool IsValid();
    }

    [HypClassBinding(Name = "LightmapVolume")]
    public class LightmapVolume : HypObject
    {
        public LightmapVolume()
        {
        }
    }
}