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

    [ClassBinding(Name = "TextureFilterMode")]
    public enum TextureFilterMode : byte
    {
        Nearest,
        Linear,
        NearestMipmapLinear,
        NearestMipmapNearest,
        LinearMipmapLinear,
        MinMaxMipmap
    }

    [ClassBinding(Name = "TextureWrapMode")]
    public enum TextureWrapMode : byte
    {
        ClampToEdge,
        ClampToBorder,
        Repeat
    }

    [ClassBinding(Name = "TextureType")]
    public enum TextureType : byte
    {
        Texture2D,
        Texture3D,
        Cubemap,
        Texture2DArray,
        CubemapArray,

        Count
    }
    [ClassBinding(Name = "ImageUsage")]
    [Flags]
    public enum ImageUsage : byte
    {
        None = 0x0,
        Sampled = 0x1,
        Storage = 0x2,
        Attachment = 0x4,
        Blended = 0x8,
        External = 0x10
    }


    [ClassBinding(Name = "TextureDesc")]
    public unsafe struct TextureDesc
    {
        public const int MaxMips = 16;

        public TextureType type;
        public TextureFormat format;
        public Vec3u extent;
        public TextureFilterMode filterModeMin;
        public TextureFilterMode filterModeMag;
        public TextureWrapMode wrapMode;
        public ushort numLayers;
        public ImageUsage imageUsage;
        public fixed uint mipOffsets[MaxMips];

        public TextureDesc()
        {
            type = TextureType.Texture2D;
            format = TextureFormat.RGBA8;
            extent = new Vec3u(1, 1, 1);
            filterModeMin = TextureFilterMode.Nearest;
            filterModeMag = TextureFilterMode.Nearest;
            wrapMode = TextureWrapMode.ClampToEdge;
            numLayers = 1;
            imageUsage = ImageUsage.Sampled;
        }
    }


    [ClassBinding(Name = "Texture")]
    public class Texture : AssetObject
    {
        public Texture()
        {
        }
    }
}