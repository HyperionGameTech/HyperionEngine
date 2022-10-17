#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_H

#include <util/Defines.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

namespace hyperion::renderer {

enum class ImageType
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

static inline BaseFormat GetBaseFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32_:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F:
        return BaseFormat::TEXTURE_FORMAT_R;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16_:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F:
        return BaseFormat::TEXTURE_FORMAT_RG;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R11G11B10F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F:
        return BaseFormat::TEXTURE_FORMAT_RGB;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R10G10B10A2:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F:
        return BaseFormat::TEXTURE_FORMAT_RGBA;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGR8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGR;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGRA;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F:
        return BaseFormat::TEXTURE_FORMAT_DEPTH;
    default:
        // undefined result
        return BaseFormat::TEXTURE_FORMAT_NONE;
    }
}

static inline UInt NumComponents(BaseFormat format)
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

static inline UInt NumComponents(InternalFormat format)
{
    return NumComponents(GetBaseFormat(format));
}

/*! \brief returns a texture format that has a shifted bytes-per-pixel count
 * e.g calling with RGB16 and num components = 4 --> RGBA16 */
static inline InternalFormat FormatChangeNumComponents(InternalFormat fmt, UInt8 new_num_components)
{
    if (new_num_components == 0) {
        return InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
    }

    new_num_components = MathUtil::Clamp(new_num_components, static_cast<UInt8>(1), static_cast<UInt8>(4));

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
    return fmt >= InternalFormat::TEXTURE_INTERNAL_FORMAT_SRGB
        && fmt < InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH;
}

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

} // namespace hyperion

#endif