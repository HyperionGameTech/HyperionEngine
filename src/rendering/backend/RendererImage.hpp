#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_H

#include <util/Defines.hpp>
#include <math/MathUtil.hpp>
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
class Image { };

template <PlatformType PLATFORM>
class StorageImage : public Image<PLATFORM> { };

template <PlatformType PLATFORM>
class StorageImage2D : public StorageImage<PLATFORM> { };

template <PlatformType PLATFORM>
class StorageImage3D : public StorageImage<PLATFORM> { };

template <PlatformType PLATFORM>
class TextureImage : public Image<PLATFORM> { };

template <PlatformType PLATFORM>
class TextureImage2D : public TextureImage<PLATFORM> { };

template <PlatformType PLATFORM>
class TextureImage3D : public TextureImage<PLATFORM> { };

template <PlatformType PLATFORM>
class TextureImageCube : public TextureImage<PLATFORM> { };

template <PlatformType PLATFORM>
class FramebufferImage : public Image<PLATFORM> { };

template <PlatformType PLATFORM>
class FramebufferImage2D : public FramebufferImage<PLATFORM> { };

template <PlatformType PLATFORM>
class FramebufferImageCube : public FramebufferImage<PLATFORM> { };

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

using Image                 = platform::Image<Platform::CURRENT>;
using StorageImage          = platform::StorageImage<Platform::CURRENT>;
using StorageImage2D        = platform::StorageImage2D<Platform::CURRENT>;
using StorageImage3D        = platform::StorageImage3D<Platform::CURRENT>;
using TextureImage          = platform::TextureImage<Platform::CURRENT>;
using TextureImage2D        = platform::TextureImage2D<Platform::CURRENT>;
using TextureImage3D        = platform::TextureImage3D<Platform::CURRENT>;
using TextureImageCube      = platform::TextureImageCube<Platform::CURRENT>;
using FramebufferImage      = platform::FramebufferImage<Platform::CURRENT>;
using FramebufferImage2D    = platform::FramebufferImage2D<Platform::CURRENT>;
using FramebufferImageCube  = platform::FramebufferImageCube<Platform::CURRENT>;

} // namespace renderer

} // namespace hyperion

#endif