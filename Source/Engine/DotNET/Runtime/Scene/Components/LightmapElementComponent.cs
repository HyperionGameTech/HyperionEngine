using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightmapElementId : uint
    {
    }

    [ClassBinding(Name="LightmapElementComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct LightmapElementComponent : IComponent
    {
        public LightmapElementId LightmapElementId;
        public uint LightmapVolumeId;
        public float LightmapVolumeWeight;
        public ulong MeshLightmapUVHash;
        private WeakHandle<LightmapVolume> _lightmapVolume;
        
        public void Dispose()
        {
            _lightmapVolume.Dispose();
        }
    }
}
