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

        public const int MaxLightmapVolumeAssignments = 4;

        public LightmapElementId LightmapElementId;

        private fixed uint _lightmapVolumeAssignments[MaxLightmapVolumeAssignments];
        private fixed float _lightmapVolumeAssignmentWeights[MaxLightmapVolumeAssignments];

        public KeyValuePair<LightmapElementId, float> this[int index]
        {
            get => new((LightmapElementId)_lightmapVolumeAssignments[index], _lightmapVolumeAssignmentWeights[index]);
            set
            {
                _lightmapVolumeAssignments[index] = (uint)value.Key;
                _lightmapVolumeAssignmentWeights[index] = value.Value;
            }
        }
        
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