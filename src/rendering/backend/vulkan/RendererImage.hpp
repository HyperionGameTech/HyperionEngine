#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class Instance;
class Device;

class Image
{
public:
    enum Type
    {
        TEXTURE_TYPE_2D = 0,
        TEXTURE_TYPE_3D = 1,
        TEXTURE_TYPE_CUBEMAP = 2
    };

    enum class BaseFormat
    {
        TEXTURE_FORMAT_NONE,
        TEXTURE_FORMAT_R,
        TEXTURE_FORMAT_RG,
        TEXTURE_FORMAT_RGB,
        TEXTURE_FORMAT_RGBA,
        
        TEXTURE_FORMAT_BGR,
        TEXTURE_FORMAT_BGRA,

        TEXTURE_FORMAT_DEPTH
    };

    enum class InternalFormat
    {
        TEXTURE_INTERNAL_FORMAT_NONE,

        TEXTURE_INTERNAL_FORMAT_R8,
        TEXTURE_INTERNAL_FORMAT_RG8,
        TEXTURE_INTERNAL_FORMAT_RGB8,
        TEXTURE_INTERNAL_FORMAT_RGBA8,
        
        TEXTURE_INTERNAL_FORMAT_B8,
        TEXTURE_INTERNAL_FORMAT_BG8,
        TEXTURE_INTERNAL_FORMAT_BGR8,
        TEXTURE_INTERNAL_FORMAT_BGRA8,

        TEXTURE_INTERNAL_FORMAT_R16,
        TEXTURE_INTERNAL_FORMAT_RG16,
        TEXTURE_INTERNAL_FORMAT_RGB16,
        TEXTURE_INTERNAL_FORMAT_RGBA16,

        TEXTURE_INTERNAL_FORMAT_R32,
        TEXTURE_INTERNAL_FORMAT_RG32,
        TEXTURE_INTERNAL_FORMAT_RGB32,
        TEXTURE_INTERNAL_FORMAT_RGBA32,

        TEXTURE_INTERNAL_FORMAT_R32_,
        TEXTURE_INTERNAL_FORMAT_RG16_,
        TEXTURE_INTERNAL_FORMAT_R11G11B10F,
        TEXTURE_INTERNAL_FORMAT_R10G10B10A2,

        TEXTURE_INTERNAL_FORMAT_R16F,
        TEXTURE_INTERNAL_FORMAT_RG16F,
        TEXTURE_INTERNAL_FORMAT_RGB16F,
        TEXTURE_INTERNAL_FORMAT_RGBA16F,

        TEXTURE_INTERNAL_FORMAT_R32F,
        TEXTURE_INTERNAL_FORMAT_RG32F,
        TEXTURE_INTERNAL_FORMAT_RGB32F,
        TEXTURE_INTERNAL_FORMAT_RGBA32F,

        TEXTURE_INTERNAL_FORMAT_SRGB, /* begin srgb */

        TEXTURE_INTERNAL_FORMAT_R8_SRGB,
        TEXTURE_INTERNAL_FORMAT_RG8_SRGB,
        TEXTURE_INTERNAL_FORMAT_RGB8_SRGB,
        TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
        
        TEXTURE_INTERNAL_FORMAT_B8_SRGB,
        TEXTURE_INTERNAL_FORMAT_BG8_SRGB,
        TEXTURE_INTERNAL_FORMAT_BGR8_SRGB,
        TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
        
        TEXTURE_INTERNAL_FORMAT_DEPTH, /* begin depth */

        TEXTURE_INTERNAL_FORMAT_DEPTH_16,
        TEXTURE_INTERNAL_FORMAT_DEPTH_24,
        TEXTURE_INTERNAL_FORMAT_DEPTH_32F
    };

    enum class FilterMode
    {
        TEXTURE_FILTER_NEAREST,
        TEXTURE_FILTER_LINEAR,
        TEXTURE_FILTER_NEAREST_MIPMAP,
        TEXTURE_FILTER_LINEAR_MIPMAP,
        TEXTURE_FILTER_MINMAX_MIPMAP
    };

    enum class WrapMode
    {
        TEXTURE_WRAP_CLAMP_TO_EDGE,
        TEXTURE_WRAP_CLAMP_TO_BORDER,
        TEXTURE_WRAP_REPEAT
    };
    
    static BaseFormat GetBaseFormat(InternalFormat);
    /* returns a texture format that has a shifted bytes-per-pixel count
     * e.g calling with RGB16 and num components = 4 --> RGBA16 */
    static InternalFormat FormatChangeNumComponents(InternalFormat, UInt8 new_num_components);

    /* Get number of components (bytes-per-pixel) of a texture format */
    static UInt NumComponents(InternalFormat format);
    /* Get number of components (bytes-per-pixel) of a texture format */
    static UInt NumComponents(BaseFormat format);

    static bool IsDepthFormat(InternalFormat fmt);
    static bool IsDepthFormat(BaseFormat fmt);
    static bool IsSRGBFormat(InternalFormat fmt);

    static VkFormat ToVkFormat(InternalFormat fmt);
    static VkImageType ToVkType(Type type);
    static VkFilter ToVkFilter(FilterMode);
    static VkSamplerAddressMode ToVkSamplerAddressMode(WrapMode);

    struct InternalInfo {
        VkImageTiling tiling;
        VkImageUsageFlags usage_flags;
    };

    Image(
        Extent3D extent,
        InternalFormat format,
        Type type,
        FilterMode filter_mode,
        const InternalInfo &internal_info,
        const UByte *bytes
    );

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;
    Image(Image &&other) noexcept;
    ~Image();

    /*
     * Create the image. No texture data will be copied.
     */
    Result Create(Device *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    Result Create(Device *device, Instance *instance, GPUMemory::ResourceState state);
    Result Destroy(Device *device);

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src
    );

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src,
        Rect src_rect,
        Rect dst_rect
    );

    Result GenerateMipmaps(
        Device *device,
        CommandBuffer *command_buffer
    );

    void CopyFromBuffer(
        CommandBuffer *command_buffer,
        const GPUBuffer *src_buffer
    ) const;

    void CopyToBuffer(
        CommandBuffer *command_buffer,
        GPUBuffer *dst_buffer
    ) const;

    /*! \brief Resize the CPU-side byte array to be at least the given capacity. To be used before uploading.
        If no bytes are currently allocated, at least the given capacity will be allocated. */
    void EnsureCapacity(SizeType size);
    
    const UByte *GetBytes() const
        { return m_bytes; }

    bool HasAssignedImageData() const
        { return m_bytes != nullptr; }

    void CopyImageData(const UByte *data, SizeType count, SizeType offset = 0)
    {
        AssertThrow(m_bytes != nullptr);
        AssertThrow(offset + count <= m_size);

        Memory::Copy(&m_bytes[offset], data, count);
    }

    bool IsDepthStencil() const;
    bool IsSRGB() const;
    void SetIsSRGB(bool srgb);

    bool IsBlended() const
        { return m_is_blended; }

    void SetIsBlended(bool is_blended)
        { m_is_blended = is_blended; }

    bool HasMipmaps() const
    {
        return m_filter_mode == FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP
            || m_filter_mode == FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
            || m_filter_mode == FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP;
    }

    UInt NumMipmaps() const
    {
        return HasMipmaps()
            ? static_cast<UInt>(MathUtil::FastLog2(MathUtil::Max(m_extent.width, m_extent.height, m_extent.depth))) + 1
            : 1;
    }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    SizeType GetByteSize() const
        { return static_cast<SizeType>(m_extent.Size())
            * static_cast<SizeType>(NumComponents(m_format))
            * static_cast<SizeType>(NumFaces()); }

    bool IsTextureCube() const
        { return m_type == TEXTURE_TYPE_CUBEMAP; }

    bool IsPanorama() const
        { return m_type == TEXTURE_TYPE_2D
            && m_extent.width == m_extent.height * 2
            && m_extent.depth == 1; }

    UInt NumFaces() const
        { return IsTextureCube() ? 6 : 1; }

    FilterMode GetFilterMode() const
        { return m_filter_mode; }

    void SetFilterMode(FilterMode filter_mode)
        { m_filter_mode = filter_mode; }

    const Extent3D &GetExtent() const
        { return m_extent; }

    GPUImageMemory *GetGPUImage()
        { return m_image; }

    const GPUImageMemory *GetGPUImage() const
        { return m_image; }

    InternalFormat GetTextureFormat() const
        { return m_format; }

    void SetTextureFormat(InternalFormat format)
        { m_format = format; }

    Type GetType() const
        { return m_type; }

    VkFormat GetImageFormat() const;
    VkImageType GetImageType() const;
    VkImageUsageFlags GetImageUsageFlags() const
        { return m_internal_info.usage_flags; }
    
private:
    Result CreateImage(Device *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info);

    Result ConvertTo32BPP(Device *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format);

    Extent3D m_extent;
    InternalFormat m_format;
    Type m_type;
    FilterMode m_filter_mode;
    UByte *m_bytes;

    bool m_is_blended;

    InternalInfo m_internal_info;

    size_t m_size;
    size_t m_bpp; // bytes per pixel
    GPUImageMemory *m_image;
};

class StorageImage : public Image
{
public:
    StorageImage(
        Extent3D extent,
        InternalFormat format,
        Type type,
        const UByte *bytes = nullptr
    ) : StorageImage(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            bytes
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        Type type,
        FilterMode filter_mode,
        const UByte *bytes = nullptr
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /*i guess?*/
                | VK_IMAGE_USAGE_STORAGE_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT
        },
        bytes
    )
    {
    }
};

class StorageImage2D : public StorageImage
{
public:
    StorageImage2D(
        Extent2D extent,
        InternalFormat format,
        const UByte *bytes = nullptr
    ) : StorageImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_2D,
        bytes
    )
    {
    }
};

class StorageImage3D : public StorageImage
{
public:
    StorageImage3D(
        Extent3D extent,
        InternalFormat format,
        const UByte *bytes = nullptr
    ) : StorageImage(
        extent,
        format,
        Type::TEXTURE_TYPE_3D,
        bytes
    )
    {
    }
};

class TextureImage : public Image
{
public:
    TextureImage(
        Extent3D extent,
        InternalFormat format,
        Type type,
        FilterMode filter_mode,
        const UByte *bytes
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo{
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                         | VK_IMAGE_USAGE_SAMPLED_BIT
        },
        bytes
    )
    {
    }
};

class TextureImage2D : public TextureImage
{
public:
    TextureImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        const UByte *bytes
    ) : TextureImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_2D,
        filter_mode,
        bytes
    )
    {
    }
};

class TextureImage3D : public TextureImage
{
public:
    TextureImage3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode filter_mode,
        const UByte *bytes
    ) : TextureImage(
        extent,
        format,
        Type::TEXTURE_TYPE_3D,
        filter_mode,
        bytes
    )
    {
    }
};

class TextureImageCube : public TextureImage
{
public:
    TextureImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        const UByte *bytes
    ) : TextureImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_CUBEMAP,
        filter_mode,
        bytes
    )
    {
    }
};

class FramebufferImage : public Image
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        Type type,
        const UByte *bytes
    ) : Image(
        extent,
        format,
        type,
        FilterMode::TEXTURE_FILTER_NEAREST,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VkImageUsageFlags((GetBaseFormat(format) == BaseFormat::TEXTURE_FORMAT_DEPTH
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                | VK_IMAGE_USAGE_SAMPLED_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT /* for mip chain */)
        },
        bytes
    )
    {
    }

    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        Type type,
        Image::FilterMode filter_mode
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VkImageUsageFlags((GetBaseFormat(format) == BaseFormat::TEXTURE_FORMAT_DEPTH
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                | VK_IMAGE_USAGE_SAMPLED_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT /* for mip chain */)
        },
        nullptr
    )
    {
    }
};

class FramebufferImage2D : public FramebufferImage
{
public:
    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        const UByte *bytes
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_2D,
        bytes
    )
    {
    }

    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        Image::FilterMode filter_mode
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_2D,
        filter_mode
    )
    {
    }
};

class FramebufferImageCube : public FramebufferImage
{
public:
    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        const UByte *bytes
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_CUBEMAP,
        bytes
    )
    {
    }

    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        Image::FilterMode filter_mode
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        Type::TEXTURE_TYPE_CUBEMAP,
        filter_mode
    )
    {
    }
};


} // namespace renderer
} // namespace hyperion

#endif
