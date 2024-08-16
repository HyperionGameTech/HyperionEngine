using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum InternalFormat : uint
    {
        None,

        R8,
        RG8,
        RGB8,
        RGBA8,

        B8,
        BG8,
        BGR8,
        BGRA8,

        R16,
        RG16,
        RGB16,
        RGBA16,

        R32,
        RG32,
        RGB32,
        RGBA32,

        R32_,
        RG16_,
        R11G11B10F,
        R10G10B10A2,

        R16F,
        RG16F,
        RGB16F,
        RGBA16F,

        R32F,
        RG32F,
        RGB32F,
        RGBA32F,

        SRGB, /* begin srgb */

        R8_SRGB,
        RG8_SRGB,
        RGB8_SRGB,
        RGBA8_SRGB,
        
        B8_SRGB,
        BG8_SRGB,
        BGR8_SRGB,
        BGRA8_SRGB,
        
        DEPTH, /* begin depth */

        DEPTH_16 = DEPTH,
        DEPTH_24,
        DEPTH_32F
    }

    public enum FilterMode : uint
    {
        Nearest,
        Linear,
        NearestMipmap,
        LinearMipmap,
        MinMaxMipmap
    }

    public enum ImageType : uint
    {
        Image2D,
        Image3D,
        ImageCube
    }

    [HypClassBinding(Name="Texture")]
    public class Texture : HypObject
    {
        public Texture()
        {
        }
        
        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }
    }
}