using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "ViewFlags")]
    [Flags]
    public enum ViewFlags : uint
    {
        None = 0x0,
        GBuffer = 0x1,

        AllWorldScenes = 0x2,

        CollectStaticEntities = 0x4,
        CollectDynamicEntities = 0x8,
        CollectAllEntities = CollectStaticEntities | CollectDynamicEntities,

        NoFrustumCulling = 0x10,

        SkipEnvProbes = 0x20,
        SkipEnvGrids = 0x40,
        SkipLights = 0x80,
        SkipLightmapVolumes = 0x100,
        SkipParticleVolumes = 0x200,
        SkipCameras = 0x400,

        NotMultiBuffered = 0x1000,

        NoDrawCalls = 0x2000,

        EnableReadback = 0x4000,

        Raytracing = 0x100000,

        MatchCameraDimensions = 0x200000,

        Default = AllWorldScenes | CollectAllEntities
    }


    [ClassBinding(Name = "View")]
    public class View : ObjectBase
    {
        public View()
        {
        }
    }
}