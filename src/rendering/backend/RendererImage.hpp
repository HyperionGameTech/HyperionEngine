/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_HPP

#include <core/Defines.hpp>

#include <math/MathUtil.hpp>
#include <math/Rect.hpp>

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

using ImageFlags = uint32;

enum ImageFlagBits : ImageFlags
{
    IMAGE_FLAGS_NONE            = 0x0,
    IMAGE_FLAGS_KEEP_IMAGE_DATA = 0x1
};

enum class ImageType : uint32
{
    TEXTURE_TYPE_2D = 0,
    TEXTURE_TYPE_3D = 1,
    TEXTURE_TYPE_CUBEMAP = 2
};

enum class BaseFormat : uint32
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

enum class InternalFormat : uint32
{
    NONE,

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
};

enum class FilterMode : uint32
{
    TEXTURE_FILTER_NEAREST,
    TEXTURE_FILTER_LINEAR,
    TEXTURE_FILTER_NEAREST_LINEAR,
    TEXTURE_FILTER_NEAREST_MIPMAP,
    TEXTURE_FILTER_LINEAR_MIPMAP,
    TEXTURE_FILTER_MINMAX_MIPMAP
};

enum class WrapMode : uint32
{
    TEXTURE_WRAP_CLAMP_TO_EDGE,
    TEXTURE_WRAP_CLAMP_TO_BORDER,
    TEXTURE_WRAP_REPEAT
};

enum class TextureMode : uint32
{
    SAMPLED,
    STORAGE
};

static inline BaseFormat GetBaseFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::R8:
    case InternalFormat::R8_SRGB:
    case InternalFormat::R32_:
    case InternalFormat::R16:
    case InternalFormat::R32:
    case InternalFormat::R16F:
    case InternalFormat::R32F:
        return BaseFormat::TEXTURE_FORMAT_R;
    case InternalFormat::RG8:
    case InternalFormat::RG8_SRGB:
    case InternalFormat::RG16_:
    case InternalFormat::RG16:
    case InternalFormat::RG32:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F:
        return BaseFormat::TEXTURE_FORMAT_RG;
    case InternalFormat::RGB8:
    case InternalFormat::RGB8_SRGB:
    case InternalFormat::R11G11B10F:
    case InternalFormat::RGB16:
    case InternalFormat::RGB32:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F:
        return BaseFormat::TEXTURE_FORMAT_RGB;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA8_SRGB:
    case InternalFormat::R10G10B10A2:
    case InternalFormat::RGBA16:
    case InternalFormat::RGBA32:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F:
        return BaseFormat::TEXTURE_FORMAT_RGBA;
    case InternalFormat::BGR8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGR;
    case InternalFormat::BGRA8:
    case InternalFormat::BGRA8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGRA;
    case InternalFormat::DEPTH_16:
    case InternalFormat::DEPTH_24:
    case InternalFormat::DEPTH_32F:
        return BaseFormat::TEXTURE_FORMAT_DEPTH;
    default:
        // undefined result
        return BaseFormat::TEXTURE_FORMAT_NONE;
    }
}

static inline uint NumComponents(BaseFormat format)
{
    switch (format) {
    case BaseFormat::TEXTURE_FORMAT_NONE: return 0;
    case BaseFormat::TEXTURE_FORMAT_R: return 1;
    case BaseFormat::TEXTURE_FORMAT_RG: return 2;
    case BaseFormat::TEXTURE_FORMAT_RGB: return 3;
    case BaseFormat::TEXTURE_FORMAT_BGR: return 3;
    case BaseFormat::TEXTURE_FORMAT_RGBA: return 4;
    case BaseFormat::TEXTURE_FORMAT_BGRA: return 4;
    case BaseFormat::TEXTURE_FORMAT_DEPTH: return 1;
    default: return 0; // undefined result
    }
}

static inline uint NumComponents(InternalFormat format)
{
    return NumComponents(GetBaseFormat(format));
}

static inline uint NumBytes(InternalFormat format)
{
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::R8_SRGB:
    case InternalFormat::RG8:
    case InternalFormat::RG8_SRGB:
    case InternalFormat::RGB8:
    case InternalFormat::RGB8_SRGB:
    case InternalFormat::BGR8_SRGB:
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA8_SRGB:
    case InternalFormat::BGRA8:
    case InternalFormat::BGRA8_SRGB:
        return 1;
    case InternalFormat::R16:
    case InternalFormat::RG16:
    case InternalFormat::RGB16:
    case InternalFormat::RGBA16:
    case InternalFormat::DEPTH_16:
        return 2;
    case InternalFormat::R32:
    case InternalFormat::RG32:
    case InternalFormat::RGB32:
    case InternalFormat::RGBA32:
    case InternalFormat::R32_:
    case InternalFormat::RG16_:
    case InternalFormat::R11G11B10F:
    case InternalFormat::R10G10B10A2:
    case InternalFormat::DEPTH_24:
    case InternalFormat::DEPTH_32F:
        return 4;
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F:
        return 2;
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F:
        return 4;
    default:
        return 0; // undefined result
    }
}

/*! \brief returns a texture format that has a shifted bytes-per-pixel count
 * e.g calling with RGB16 and num components = 4 --> RGBA16 */
static inline InternalFormat FormatChangeNumComponents(InternalFormat fmt, uint8 new_num_components)
{
    if (new_num_components == 0) {
        return InternalFormat::NONE;
    }

    new_num_components = MathUtil::Clamp(new_num_components, static_cast<uint8>(1), static_cast<uint8>(4));

    int current_num_components = int(NumComponents(fmt));

    return InternalFormat(int(fmt) + int(new_num_components) - current_num_components);
}

static inline bool IsDepthFormat(BaseFormat fmt)
{
    return fmt == BaseFormat::TEXTURE_FORMAT_DEPTH;
}

static inline bool IsDepthFormat(InternalFormat fmt)
{
    return IsDepthFormat(GetBaseFormat(fmt));
}

static inline bool IsSRGBFormat(InternalFormat fmt)
{
    return fmt >= InternalFormat::SRGB && fmt < InternalFormat::DEPTH;
}

namespace platform {

template <PlatformType PLATFORM>
struct ImagePlatformImpl;

template <PlatformType PLATFORM>
class Image
{
public:
    friend struct ImagePlatformImpl<PLATFORM>;

    HYP_API Image(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data,
        ImageFlags flags = IMAGE_FLAGS_NONE
    );

    Image(const Image &other)               = delete;
    Image &operator=(const Image &other)    = delete;
    HYP_API Image(Image &&other) noexcept;
    HYP_API Image &operator=(Image &&other) noexcept;
    HYP_API ~Image();

    [[nodiscard]]
    HYP_FORCE_INLINE
    ImagePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ImagePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    /*
     * Create the image. No texture data will be copied.
     */
    HYP_API Result Create(Device<PLATFORM> *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    HYP_API Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance, ResourceState state);
    HYP_API Result Destroy(Device<PLATFORM> *device);

    HYP_API bool IsCreated() const;

    HYP_API ResourceState GetResourceState() const;
    HYP_API void SetResourceState(ResourceState new_state);

    HYP_API ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;
    HYP_API void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        ResourceState new_state,
        ImageSubResourceFlagBits flags = ImageSubResourceFlags::IMAGE_SUB_RESOURCE_FLAGS_COLOR
    );

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API void InsertSubResourceBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect,
        uint src_mip,
        uint dst_mip
    );

    HYP_API Result GenerateMipmaps(
        Device<PLATFORM> *device,
        CommandBuffer<PLATFORM> *command_buffer
    );

    HYP_API void CopyFromBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        const GPUBuffer<PLATFORM> *src_buffer
    ) const;

    HYP_API void CopyToBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        GPUBuffer<PLATFORM> *dst_buffer
    ) const;

    HYP_API ByteBuffer ReadBack(Device<PLATFORM> *device, Instance<PLATFORM> *instance) const;

    bool IsRWTexture() const
        { return m_is_rw_texture; }

    void SetIsRWTexture(bool is_rw_texture)
        { m_is_rw_texture = is_rw_texture; }

    bool IsAttachmentTexture() const
        { return m_is_attachment_texture; }

    void SetIsAttachmentTexture(bool is_attachment_texture)
        { m_is_attachment_texture = is_attachment_texture; }

    StreamedData *GetStreamedData() const
        { return m_streamed_data.Get(); }

    bool HasAssignedImageData() const
        { return m_streamed_data != nullptr && !m_streamed_data->IsNull(); }

    void CopyImageData(const ByteBuffer &byte_buffer)
        { m_streamed_data.Reset(new MemoryStreamedData(byte_buffer)); }

    bool IsDepthStencil() const
        { return IsDepthFormat(m_format); }

    bool IsSRGB() const
        { return IsSRGBFormat(m_format); }

    void SetIsSRGB(bool srgb)
    {
        const bool is_srgb = IsSRGB();

        if (srgb == is_srgb) {
            return;
        }

        const auto internal_format = m_format;

        if (is_srgb) {
            m_format = InternalFormat(int(internal_format) - int(InternalFormat::SRGB));

            return;
        }

        const auto to_srgb_format = InternalFormat(int(InternalFormat::SRGB) + int(internal_format));

        if (!IsSRGBFormat(to_srgb_format)) {
            DebugLog(
                LogType::Warn,
                "No SRGB counterpart for image type (%d)\n",
                static_cast<int>(internal_format)
            );
        }

        m_format = to_srgb_format;
    }

    bool IsBlended() const
        { return m_is_blended; }

    void SetIsBlended(bool is_blended)
        { m_is_blended = is_blended; }

    bool HasMipmaps() const
    {
        return m_min_filter_mode == FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP
            || m_min_filter_mode == FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
            || m_min_filter_mode == FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP;
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
    uint GetByteSize() const
        { return uint(m_extent.Size())
            * NumComponents(m_format)
            * NumBytes(m_format)
            * NumFaces(); }

    uint8 GetBPP() const
        { return m_bpp; }

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

    FilterMode GetMinFilterMode() const
        { return m_min_filter_mode; }

    void SetMinFilterMode(FilterMode filter_mode)
        { m_min_filter_mode = filter_mode; }

    FilterMode GetMagFilterMode() const
        { return m_mag_filter_mode; }

    void SetMagFilterMode(FilterMode filter_mode)
        { m_mag_filter_mode = filter_mode; }

    const Extent3D &GetExtent() const
        { return m_extent; }

    InternalFormat GetTextureFormat() const
        { return m_format; }

    void SetTextureFormat(InternalFormat format)
        { m_format = format; }

    ImageType GetType() const
        { return m_type; }

protected:
    ImageFlags  m_flags;
    
private:

    ImagePlatformImpl<PLATFORM>                 m_platform_impl;

    Extent3D                                    m_extent;
    InternalFormat                              m_format;
    ImageType                                   m_type;
    FilterMode                                  m_min_filter_mode;
    FilterMode                                  m_mag_filter_mode;
    UniquePtr<StreamedData>                     m_streamed_data;

    bool                                        m_is_blended;
    uint                                        m_num_layers;
    bool                                        m_is_rw_texture;
    bool                                        m_is_attachment_texture;

    SizeType                                    m_size;
    SizeType                                    m_bpp; // bytes per pixel
};

template <PlatformType PLATFORM>
class StorageImage : public Image<PLATFORM>
{
public:
    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : Image<PLATFORM>(
            extent,
            format,
            type,
            min_filter_mode,
            mag_filter_mode,
            std::move(streamed_data)
        )
    {
        Image<PLATFORM>::SetIsRWTexture(true);
    }

    StorageImage()
        : StorageImage(
            Extent3D { 1, 1, 1 },
            InternalFormat::RGBA16F,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_NEAREST,
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
            FilterMode::TEXTURE_FILTER_NEAREST,
            std::move(streamed_data)
        )
    {
    }

    StorageImage(const StorageImage &other)             = delete;
    StorageImage &operator=(const StorageImage &other)  = delete;

    StorageImage(StorageImage &&other) noexcept
        : Image<PLATFORM>(std::move(other))
    {
    }

    StorageImage &operator=(StorageImage &&other) noexcept
    {
        Image<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~StorageImage()                                     = default;
};

template <PlatformType PLATFORM>
class StorageImage2D : public StorageImage<PLATFORM>
{
public:
    StorageImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage<PLATFORM>(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        std::move(streamed_data)
    )
    {
    }
    
    StorageImage2D(const StorageImage2D &other)             = delete;
    StorageImage2D &operator=(const StorageImage2D &other)  = delete;

    StorageImage2D(StorageImage2D &&other) noexcept
        : StorageImage<PLATFORM>(std::move(other))
    {
    }

    StorageImage2D &operator=(StorageImage2D &&other) noexcept
    {
        StorageImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~StorageImage2D()                                       = default;
};

template <PlatformType PLATFORM>
class StorageImage3D : public StorageImage<PLATFORM>
{
public:
    StorageImage3D(
        Extent3D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage<PLATFORM>(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        std::move(streamed_data)
    )
    {
    }

    StorageImage3D(const StorageImage3D &other)             = delete;
    StorageImage3D &operator=(const StorageImage3D &other)  = delete;

    StorageImage3D(StorageImage3D &&other) noexcept
        : StorageImage<PLATFORM>(std::move(other))
    {
    }

    StorageImage3D &operator=(StorageImage3D &&other) noexcept
    {
        StorageImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~StorageImage3D()                                       = default;
};

template <PlatformType PLATFORM>
class TextureImage : public Image<PLATFORM>
{
public:
    TextureImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image<PLATFORM>(
        extent,
        format,
        type,
        min_filter_mode,
        mag_filter_mode,
        std::move(streamed_data)
    )
    {
    }

    TextureImage(const TextureImage &other)             = delete;
    TextureImage &operator=(const TextureImage &other)  = delete;

    TextureImage(TextureImage &&other) noexcept
        : Image<PLATFORM>(std::move(other))
    {
    }

    TextureImage &operator=(TextureImage &&other) noexcept
    {
        Image<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~TextureImage()                                     = default;
};

template <PlatformType PLATFORM>
class TextureImage2D : public TextureImage<PLATFORM>
{
public:
    TextureImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            min_filter_mode,
            mag_filter_mode,
            std::move(streamed_data)
        )
    {
    }

    TextureImage2D(const TextureImage2D &other)             = delete;
    TextureImage2D &operator=(const TextureImage2D &other)  = delete;

    TextureImage2D(TextureImage2D &&other) noexcept
        : TextureImage<PLATFORM>(std::move(other))
    {
    }

    TextureImage2D &operator=(TextureImage2D &&other) noexcept
    {
        TextureImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~TextureImage2D()                                       = default;
};

template <PlatformType PLATFORM>
class TextureImage3D : public TextureImage<PLATFORM>
{
public:
    TextureImage3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage<PLATFORM>(
            extent,
            format,
            ImageType::TEXTURE_TYPE_3D,
            min_filter_mode,
            mag_filter_mode,
            std::move(streamed_data)
        )
    {
    }

    TextureImage3D(const TextureImage3D &other)             = delete;
    TextureImage3D &operator=(const TextureImage3D &other)  = delete;

    TextureImage3D(TextureImage3D &&other) noexcept
        : TextureImage<PLATFORM>(std::move(other))
    {
    }

    TextureImage3D &operator=(TextureImage3D &&other) noexcept
    {
        TextureImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~TextureImage3D()                                       = default;
};

template <PlatformType PLATFORM>
class TextureImageCube : public TextureImage<PLATFORM>
{
public:
    TextureImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            min_filter_mode,
            mag_filter_mode,
            std::move(streamed_data)
        )
    {
    }

    TextureImageCube(const TextureImageCube &other)             = delete;
    TextureImageCube &operator=(const TextureImageCube &other)  = delete;

    TextureImageCube(TextureImageCube &&other) noexcept
        : TextureImage<PLATFORM>(std::move(other))
    {
    }

    TextureImageCube &operator=(TextureImageCube &&other) noexcept
    {
        TextureImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~TextureImageCube()                                         = default;
};

template <PlatformType PLATFORM>
class FramebufferImage : public Image<PLATFORM>
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image<PLATFORM>(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            std::move(streamed_data)
        )
    {
        Image<PLATFORM>::SetIsAttachmentTexture(true);
    }

    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : Image<PLATFORM>(
            extent,
            format,
            type,
            min_filter_mode,
            mag_filter_mode,
            nullptr
        )
    {
        Image<PLATFORM>::SetIsAttachmentTexture(true);
    }
};

template <PlatformType PLATFORM>
class FramebufferImage2D : public FramebufferImage<PLATFORM>
{
public:
    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage<PLATFORM>(
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
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }
};

template <PlatformType PLATFORM>
class FramebufferImageCube : public FramebufferImage<PLATFORM>
{
public:
    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage<PLATFORM>(
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
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }
};

} // namespace platform

} // namespace hyperion::renderer

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImage.hpp>
#else
#error Unsupported rendering backend
#endif

// to reduce noise, pull the above enum classes into hyperion namespace
namespace hyperion {

using renderer::ImageType;
using renderer::BaseFormat;
using renderer::InternalFormat;
using renderer::FilterMode;
using renderer::WrapMode;
using renderer::TextureMode;

namespace renderer {

using Image = platform::Image<Platform::CURRENT>;
using StorageImage = platform::StorageImage<Platform::CURRENT>;
using StorageImage2D = platform::StorageImage2D<Platform::CURRENT>;
using StorageImage3D = platform::StorageImage3D<Platform::CURRENT>;
using TextureImage = platform::TextureImage<Platform::CURRENT>;
using TextureImage2D = platform::TextureImage2D<Platform::CURRENT>;
using TextureImage3D = platform::TextureImage3D<Platform::CURRENT>;
using TextureImageCube = platform::TextureImageCube<Platform::CURRENT>;
using FramebufferImage = platform::FramebufferImage<Platform::CURRENT>;
using FramebufferImage2D = platform::FramebufferImage2D<Platform::CURRENT>;
using FramebufferImageCube = platform::FramebufferImageCube<Platform::CURRENT>;

} // namespace renderer

} // namespace hyperion

#endif