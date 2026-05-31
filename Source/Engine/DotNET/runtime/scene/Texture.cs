using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum TextureFormat : uint
    {
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

        /* begin srgb */
        R8_SRGB,
        RG8_SRGB,
        RGB8_SRGB,
        RGBA8_SRGB,

        B8_SRGB,
        BG8_SRGB,
        BGR8_SRGB,
        BGRA8_SRGB,

        /* begin depth */
        D16,
        D24_S8,
        D32F,
        D32F_S8
    }

    public enum TextureFilterMode : uint
    {
        Nearest,
        Linear,
        NearestMipmap,
        LinearMipmap,
        MinMaxMipmap
    }

    public enum TextureType : uint
    {
        Image2D,
        Image3D,
        ImageCube
    }

    [ClassBinding(Name = "Texture")]
    public class Texture : AssetObject
    {
        public Texture()
        {
        }
    }
}