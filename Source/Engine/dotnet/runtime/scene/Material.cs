using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum TextureKey : ulong
    {
        None = 0x0,
        DiffuseMap = 0x1,
        NormalMap = 0x2,
        ParallaxMap = 0x4,
        MetalnessMap = 0x8,
        RoughnessMap = 0x10,
        AOMap = 0x20,
        SkyboxMap = 0x40,
        ColorMap = 0x80,
        PositionMap = 0x100,
        DataMap = 0x200,
        SSAOMap = 0x400,
        TangentMap = 0x800,
        BitangentMap = 0x1000,
        DepthMap = 0x2000
    }

    [ClassBinding(Name = "Material")]
    public class Material : AssetObject
    {
        public Material()
        {
        }
    }
}