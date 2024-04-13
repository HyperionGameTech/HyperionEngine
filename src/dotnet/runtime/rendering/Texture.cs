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

    public class Texture : IDisposable
    {
        private ManagedHandle handle;

        public Texture()
        {
            handle = Texture_Create();
        }

        public Texture(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Texture_GetTypeID());
        }

        public void Dispose()
        {
            handle.DecRef(Texture_GetTypeID());
        }

        public void Init()
        {
            Texture_Init(handle);
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public InternalFormat InternalFormat
        {
            get
            {
                return Texture_GetInternalFormat(handle);
            }
        }

        public FilterMode FilterMode
        {
            get
            {
                return Texture_GetFilterMode(handle);
            }
        }

        public ImageType ImageType
        {
            get
            {
                return Texture_GetImageType(handle);
            }
        }

        [DllImport("hyperion", EntryPoint = "Texture_GetTypeID")]
        private static extern TypeID Texture_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Texture_Create")]
        private static extern ManagedHandle Texture_Create();

        [DllImport("hyperion", EntryPoint = "Texture_Init")]
        private static extern void Texture_Init(ManagedHandle texture);

        [DllImport("hyperion", EntryPoint = "Texture_GetInternalFormat")]
        private static extern InternalFormat Texture_GetInternalFormat(ManagedHandle texture);

        [DllImport("hyperion", EntryPoint = "Texture_GetFilterMode")]
        private static extern FilterMode Texture_GetFilterMode(ManagedHandle texture);

        [DllImport("hyperion", EntryPoint = "Texture_GetImageType")]
        private static extern ImageType Texture_GetImageType(ManagedHandle texture);
    }
}