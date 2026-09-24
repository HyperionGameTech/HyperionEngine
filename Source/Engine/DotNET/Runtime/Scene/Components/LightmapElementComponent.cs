using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightmapElementId : uint
    {
    }

    [ClassBinding(Name="LightmapElementComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public unsafe ref struct LightmapElementComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(LightmapElementComponent));

        public LightmapElementId LightmapElementId;
        public uint LightmapVolumeId;
        public float LightmapVolumeWeight;
        public ulong MeshLightmapUVHash;


        public void Dispose()
        {
            // Do nothing
        }

        public IntPtr NativeAddress
        {
            get
            {
                fixed (LightmapElementComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}
