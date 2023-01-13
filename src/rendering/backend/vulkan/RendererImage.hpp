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
    struct InternalInfo
    {
        VkImageTiling tiling;
        VkImageUsageFlags usage_flags;
    };

    Image(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        const InternalInfo &internal_info,
        const UByte *bytes
    );

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other) noexcept;
    ~Image();

    /*
     * Create the image. No texture data will be copied.
     */
    Result Create(Device *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    Result Create(Device *device, Instance *instance, ResourceState state);
    Result Destroy(Device *device);

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src
    );

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src,
        ImageRect src_rect,
        ImageRect dst_rect
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
        { return m_type == ImageType::TEXTURE_TYPE_CUBEMAP; }

    bool IsPanorama() const
        { return m_type == ImageType::TEXTURE_TYPE_2D
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

    ImageType GetType() const
        { return m_type; }
    
private:
    Result CreateImage(
        Device *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info
    );

    Result ConvertTo32BPP(
        Device *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format
    );

    Extent3D m_extent;
    InternalFormat m_format;
    ImageType m_type;
    FilterMode m_filter_mode;
    UByte *m_bytes;

    bool m_is_blended;

    InternalInfo m_internal_info;

    SizeType m_size;
    SizeType m_bpp; // bytes per pixel
    GPUImageMemory *m_image;
};

class StorageImage : public Image
{
public:
    StorageImage()
        : StorageImage(
            Extent3D { 1, 1, 1 },
            InternalFormat::RGBA16F,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_NEAREST,
            nullptr
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
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
        ImageType type,
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

    StorageImage(const StorageImage &other) = delete;
    StorageImage &operator=(const StorageImage &other) = delete;

    StorageImage(StorageImage &&other) noexcept
        : Image(std::move(other))
    {
    }

    StorageImage &operator=(StorageImage &&other) noexcept
    {
        Image::operator=(std::move(other));

        return *this;
    }

    ~StorageImage() = default;
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
        ImageType::TEXTURE_TYPE_2D,
        bytes
    )
    {
    }
    
    StorageImage2D(const StorageImage2D &other) = delete;
    StorageImage2D &operator=(const StorageImage2D &other) = delete;

    StorageImage2D(StorageImage2D &&other) noexcept
        : StorageImage(std::move(other))
    {
    }

    StorageImage2D &operator=(StorageImage2D &&other) noexcept
    {
        StorageImage::operator=(std::move(other));

        return *this;
    }

    ~StorageImage2D() = default;
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
        ImageType::TEXTURE_TYPE_3D,
        bytes
    )
    {
    }

    StorageImage3D(const StorageImage3D &other) = delete;
    StorageImage3D &operator=(const StorageImage3D &other) = delete;

    StorageImage3D(StorageImage3D &&other) noexcept
        : StorageImage(std::move(other))
    {
    }

    StorageImage3D &operator=(StorageImage3D &&other) noexcept
    {
        StorageImage::operator=(std::move(other));

        return *this;
    }

    ~StorageImage3D() = default;
};

class TextureImage : public Image
{
public:
    TextureImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
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

    TextureImage(const TextureImage &other) = delete;
    TextureImage &operator=(const TextureImage &other) = delete;

    TextureImage(TextureImage &&other) noexcept
        : Image(std::move(other))
    {
    }

    TextureImage &operator=(TextureImage &&other) noexcept
    {
        Image::operator=(std::move(other));

        return *this;
    }

    ~TextureImage() = default;
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
        ImageType::TEXTURE_TYPE_2D,
        filter_mode,
        bytes
    )
    {
    }

    TextureImage2D(const TextureImage2D &other) = delete;
    TextureImage2D &operator=(const TextureImage2D &other) = delete;

    TextureImage2D(TextureImage2D &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImage2D &operator=(TextureImage2D &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImage2D() = default;
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
        ImageType::TEXTURE_TYPE_3D,
        filter_mode,
        bytes
    )
    {
    }

    TextureImage3D(const TextureImage3D &other) = delete;
    TextureImage3D &operator=(const TextureImage3D &other) = delete;

    TextureImage3D(TextureImage3D &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImage3D &operator=(TextureImage3D &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImage3D() = default;
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
        ImageType::TEXTURE_TYPE_CUBEMAP,
        filter_mode,
        bytes
    )
    {
    }

    TextureImageCube(const TextureImageCube &other) = delete;
    TextureImageCube &operator=(const TextureImageCube &other) = delete;

    TextureImageCube(TextureImageCube &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImageCube &operator=(TextureImageCube &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImageCube() = default;
};

class FramebufferImage : public Image
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
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
        ImageType type,
        FilterMode filter_mode
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
            ImageType::TEXTURE_TYPE_2D,
            bytes
        )
    {
    }

    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode
    ) : FramebufferImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
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
        ImageType::TEXTURE_TYPE_CUBEMAP,
        bytes
    )
    {
    }

    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        filter_mode
    )
    {
    }
};


} // namespace renderer
} // namespace hyperion

#endif
