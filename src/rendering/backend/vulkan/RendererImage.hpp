#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/Platform.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

using v2::StreamedData;
using v2::MemoryStreamedData;


namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class CommandBuffer;

template <>
class Image<Platform::VULKAN>
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
        UniquePtr<StreamedData> &&streamed_data,
        ImageFlags flags = IMAGE_FLAGS_NONE
    );

    Image(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        const InternalInfo &internal_info,
        UniquePtr<StreamedData> &&streamed_data,
        ImageFlags flags = IMAGE_FLAGS_NONE
    );

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other) noexcept;
    ~Image();

    /*
     * Create the image. No texture data will be copied.
     */
    Result Create(Device<Platform::VULKAN> *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    Result Create(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance, ResourceState state);
    Result Destroy(Device<Platform::VULKAN> *device);

    Result Blit(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const Image *src
    );

    Result Blit(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const Image *src,
        Rect src_rect,
        Rect dst_rect
    );

    Result Blit(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const Image *src,
        Rect src_rect,
        Rect dst_rect,
        uint src_mip,
        uint dst_mip
    );

    Result GenerateMipmaps(
        Device<Platform::VULKAN> *device,
        CommandBuffer<Platform::VULKAN> *command_buffer
    );

    void CopyFromBuffer(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const GPUBuffer<Platform::VULKAN> *src_buffer
    ) const;

    void CopyToBuffer(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        GPUBuffer<Platform::VULKAN> *dst_buffer
    ) const;

    ByteBuffer ReadBack(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance) const;

    bool IsRWTexture() const
        { return m_is_rw_texture; }

    void SetIsRWTexture(bool is_rw_texture)
        { m_is_rw_texture = is_rw_texture; }

    bool IsAttachmentTexture() const
        { return m_is_attachment_texture; }

    void SetIsAttachmentTexture(bool is_attachment_texture)
        { m_is_attachment_texture = is_attachment_texture; }

    const StreamedData *GetStreamedData() const
        { return m_streamed_data.Get(); }

    bool HasAssignedImageData() const
        { return m_streamed_data != nullptr && !m_streamed_data->IsNull(); }

    void CopyImageData(const ByteBuffer &byte_buffer)
    {
        m_streamed_data.Reset(new MemoryStreamedData(byte_buffer));
    }

    // void CopyImageData(const ubyte *data, SizeType count, SizeType offset = 0)
    // {
    //     const SizeType min_size = offset + count;

    //     // Copy current data to a new buffer.
    //     if (HasAssignedImageData()) { 
    //         ByteBuffer copy_bytebuffer = m_streamed_data->Load().Copy();

    //         if (copy_bytebuffer.Size() < min_size) {
    //             copy_bytebuffer.SetSize(min_size);
    //         }

    //         // Copy new data into the buffer, at the given offset.
    //         Memory::MemCpy(copy_bytebuffer.Data() + offset, data, count);

    //         m_streamed_data.Reset(new MemoryStreamedData(std::move(copy_bytebuffer)));
    //     } else {
    //         ByteBuffer new_bytebuffer(min_size);

    //         Memory::MemCpy(new_bytebuffer.Data() + offset, data, count);

    //         m_streamed_data.Reset(new MemoryStreamedData(std::move(new_bytebuffer)));
    //     }
    // }

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

    uint NumMipmaps() const
    {
        return HasMipmaps()
            ? uint(MathUtil::FastLog2(MathUtil::Max(m_extent.width, m_extent.height, m_extent.depth))) + 1
            : 1;
    }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    SizeType GetByteSize() const
        { return m_extent.Size()
            * SizeType(NumComponents(m_format))
            * SizeType(NumBytes(m_format))
            * SizeType(NumFaces()); }

    bool IsTextureCube() const
        { return m_type == ImageType::TEXTURE_TYPE_CUBEMAP; }

    bool IsPanorama() const
        { return m_type == ImageType::TEXTURE_TYPE_2D
            && m_extent.width == m_extent.height * 2
            && m_extent.depth == 1; }

    bool IsTextureArray() const
        { return !IsTextureCube() && m_num_layers > 1; }

    bool IsTexture3D() const
        { return m_type == ImageType::TEXTURE_TYPE_3D; }

    bool IsTexture2D() const
        { return m_type == ImageType::TEXTURE_TYPE_2D; }

    uint NumLayers() const
        { return m_num_layers; }

    void SetNumLayers(uint num_layers)
    {
        m_num_layers = num_layers;
        m_size = GetByteSize();
    }

    uint NumFaces() const
        { return IsTextureCube()
            ? 6
            : IsTextureArray()
                ? m_num_layers
                : 1; }

    FilterMode GetFilterMode() const
        { return m_filter_mode; }

    void SetFilterMode(FilterMode filter_mode)
        { m_filter_mode = filter_mode; }

    const Extent3D &GetExtent() const
        { return m_extent; }

    GPUImageMemory<Platform::VULKAN> *GetGPUImage()
        { return m_image; }

    const GPUImageMemory<Platform::VULKAN> *GetGPUImage() const
        { return m_image; }

    InternalFormat GetTextureFormat() const
        { return m_format; }

    void SetTextureFormat(InternalFormat format)
        { m_format = format; }

    ImageType GetType() const
        { return m_type; }

protected:
    ImageFlags m_flags;
    
private:
    Result CreateImage(
        Device<Platform::VULKAN> *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info
    );

    Result ConvertTo32BPP(
        Device<Platform::VULKAN> *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format
    );

    Extent3D                            m_extent;
    InternalFormat                      m_format;
    ImageType                           m_type;
    FilterMode                          m_filter_mode;
    UniquePtr<StreamedData>             m_streamed_data;

    bool                                m_is_blended;
    uint                                m_num_layers;
    bool                                m_is_rw_texture;
    bool                                m_is_attachment_texture;

    InternalInfo                        m_internal_info;

    SizeType                            m_size;
    SizeType                            m_bpp; // bytes per pixel
    GPUImageMemory<Platform::VULKAN>    *m_image;
};

template <>
class StorageImage<Platform::VULKAN> : public Image<Platform::VULKAN>
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
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            std::move(streamed_data)
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : Image<Platform::VULKAN>(
            extent,
            format,
            type,
            filter_mode,
            std::move(streamed_data)
        )
    {
        SetIsRWTexture(true);
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

template <>
class StorageImage2D<Platform::VULKAN> : public StorageImage<Platform::VULKAN>
{
public:
    StorageImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        std::move(streamed_data)
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

template <>
class StorageImage3D<Platform::VULKAN> : public StorageImage<Platform::VULKAN>
{
public:
    StorageImage3D(
        Extent3D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        std::move(streamed_data)
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

template <>
class TextureImage<Platform::VULKAN> : public Image<Platform::VULKAN>
{
public:
    TextureImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image<Platform::VULKAN>(
        extent,
        format,
        type,
        filter_mode,
        std::move(streamed_data)
    )
    {
    }

    TextureImage(const TextureImage &other) = delete;
    TextureImage &operator=(const TextureImage &other) = delete;

    TextureImage(TextureImage &&other) noexcept
        : Image<Platform::VULKAN>(std::move(other))
    {
    }

    TextureImage &operator=(TextureImage &&other) noexcept
    {
        Image<Platform::VULKAN>::operator=(std::move(other));

        return *this;
    }

    ~TextureImage() = default;
};

template <>
class TextureImage2D<Platform::VULKAN> : public TextureImage<Platform::VULKAN>
{
public:
    TextureImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            filter_mode,
            std::move(streamed_data)
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

template <>
class TextureImage3D<Platform::VULKAN> : public TextureImage<Platform::VULKAN>
{
public:
    TextureImage3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
            extent,
            format,
            ImageType::TEXTURE_TYPE_3D,
            filter_mode,
            std::move(streamed_data)
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

template <>
class TextureImageCube<Platform::VULKAN> : public TextureImage<Platform::VULKAN>
{
public:
    TextureImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            filter_mode,
            std::move(streamed_data)
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

template <>
class FramebufferImage<Platform::VULKAN> : public Image<Platform::VULKAN>
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image<Platform::VULKAN>(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            std::move(streamed_data)
        )
    {
        SetIsAttachmentTexture(true);
    }

    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode
    ) : Image<Platform::VULKAN>(
            extent,
            format,
            type,
            filter_mode,
            nullptr
        )
    {
        SetIsAttachmentTexture(true);
    }
};

template <>
class FramebufferImage2D<Platform::VULKAN> : public FramebufferImage<Platform::VULKAN>
{
public:
    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            std::move(streamed_data)
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

template <>
class FramebufferImageCube<Platform::VULKAN> : public FramebufferImage<Platform::VULKAN>
{
public:
    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            std::move(streamed_data)
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

} // namespace platform

} // namespace renderer
} // namespace hyperion

#endif
