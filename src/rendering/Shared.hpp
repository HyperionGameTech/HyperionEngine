/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Float16.hpp>

#include <core/math/Vector2.hpp>

#include <util/EnumOptions.hpp>

namespace Hyperion {

HYP_ENUM()
enum ImageUsage : uint8
{
    IU_NONE = 0x0,
    IU_SAMPLED = 0x1,
    IU_STORAGE = 0x2,
    IU_ATTACHMENT = 0x4,
    IU_BLENDED = 0x8,
    IU_EXTERNAL = 0x10
};

HYP_MAKE_ENUM_FLAGS(ImageUsage);

HYP_ENUM()
enum ImageSupport : uint8
{
    IS_SRV,
    IS_UAV,
    IS_DEPTH
};

HYP_ENUM()
enum DefaultImageFormat : uint8
{
    DIF_NONE,
    DIF_COLOR,
    DIF_DEPTH,
    DIF_NORMALS,
    DIF_STORAGE
};

HYP_ENUM()
enum TextureType : uint8
{
    TT_INVALID = uint8(-1),

    TT_TEX2D = 0,
    TT_TEX3D = 1,
    TT_CUBEMAP = 2,
    TT_TEX2D_ARRAY = 3,
    TT_CUBEMAP_ARRAY = 4,

    TT_MAX
};

HYP_ENUM()
enum TextureBaseFormat : uint8
{
    TFB_NONE,
    TFB_R,
    TFB_RG,
    TFB_RGB,
    TFB_RGBA,
    TFB_BGR,
    TFB_BGRA,
    TFB_DEPTH
};

HYP_ENUM()
enum TextureFormat : uint8
{
    TF_NONE,

    TF_R8,
    TF_RG8,
    TF_RGB8,
    TF_RGBA8,

    TF_B8,
    TF_BG8,
    TF_BGR8,
    TF_BGRA8,

    TF_R16,
    TF_RG16,
    TF_RGB16,
    TF_RGBA16,

    TF_R32,
    TF_RG32,
    TF_RGB32,
    TF_RGBA32,

    TF_R32_,
    TF_RG16_,
    TF_R11G11B10F,
    TF_R10G10B10A2,

    TF_R16F,
    TF_RG16F,
    TF_RGB16F,
    TF_RGBA16F,

    TF_R32F,
    TF_RG32F,
    TF_RGB32F,
    TF_RGBA32F,

    TF_SRGB, /* begin srgb */

    TF_R8_SRGB,
    TF_RG8_SRGB,
    TF_RGB8_SRGB,
    TF_RGBA8_SRGB,

    TF_B8_SRGB,
    TF_BG8_SRGB,
    TF_BGR8_SRGB,
    TF_BGRA8_SRGB,

    TF_DEPTH, /* begin depth */

    TF_DEPTH_16 = TF_DEPTH,
    TF_DEPTH_24,
    TF_DEPTH_32F
};

HYP_ENUM()
enum TextureFilterMode : uint8
{
    TFM_NEAREST,
    TFM_LINEAR,
    TFM_NEAREST_LINEAR,
    TFM_NEAREST_MIPMAP,
    TFM_LINEAR_MIPMAP,
    TFM_MINMAX_MIPMAP
};

HYP_ENUM()
enum TextureWrapMode : uint8
{
    TWM_CLAMP_TO_EDGE,
    TWM_CLAMP_TO_BORDER,
    TWM_REPEAT
};

HYP_ENUM()
enum ResourceState : uint8
{
    RS_UNDEFINED,
    RS_PRE_INITIALIZED,
    RS_COMMON,
    RS_VERTEX_BUFFER,
    RS_CONSTANT_BUFFER,
    RS_INDEX_BUFFER,
    RS_RENDER_TARGET,
    RS_UNORDERED_ACCESS,
    RS_DEPTH_STENCIL,
    RS_SHADER_RESOURCE,
    RS_STREAM_OUT,
    RS_INDIRECT_ARG,
    RS_COPY_DST,
    RS_COPY_SRC,
    RS_RESOLVE_DST,
    RS_RESOLVE_SRC,
    RS_PRESENT,
    RS_READ_GENERIC,
    RS_PREDICATION
};

namespace TextureUtils {

static inline constexpr TextureBaseFormat GetBaseFormat(TextureFormat fmt)
{
    switch (fmt)
    {
    case TF_R8:
    case TF_R8_SRGB:
    case TF_R32_:
    case TF_R16:
    case TF_R32:
    case TF_R16F:
    case TF_R32F:
        return TFB_R;
    case TF_RG8:
    case TF_RG8_SRGB:
    case TF_RG16_:
    case TF_RG16:
    case TF_RG32:
    case TF_RG16F:
    case TF_RG32F:
        return TFB_RG;
    case TF_RGB8:
    case TF_RGB8_SRGB:
    case TF_RGB16:
    case TF_RGB32:
    case TF_RGB16F:
    case TF_RGB32F:
        return TFB_RGB;
    case TF_RGBA8:
    case TF_RGBA8_SRGB:
    case TF_R11G11B10F: // treat R11G11B10F as RGBA so it is correctly calculated as 4 bytes per pixel.
    case TF_R10G10B10A2:
    case TF_RGBA16:
    case TF_RGBA32:
    case TF_RGBA16F:
    case TF_RGBA32F:
        return TFB_RGBA;
    case TF_BGR8_SRGB:
        return TFB_BGR;
    case TF_BGRA8:
    case TF_BGRA8_SRGB:
        return TFB_BGRA;
    case TF_DEPTH_16:
    case TF_DEPTH_24:
    case TF_DEPTH_32F:
        return TFB_DEPTH;
    default:
        // undefined result
        return TFB_NONE;
    }
}

static inline constexpr uint32 NumComponents(TextureBaseFormat format)
{
    switch (format)
    {
    case TFB_NONE:
        return 0;
    case TFB_R:
        return 1;
    case TFB_RG:
        return 2;
    case TFB_RGB:
        return 3;
    case TFB_BGR:
        return 3;
    case TFB_RGBA:
        return 4;
    case TFB_BGRA:
        return 4;
    case TFB_DEPTH:
        return 1;
    default:
        return 0; // undefined result
    }
}

static inline constexpr uint32 NumComponents(TextureFormat format)
{
    return NumComponents(GetBaseFormat(format));
}

static inline constexpr uint32 BytesPerComponent(TextureFormat format)
{
    switch (format)
    {
    case TF_R8:
    case TF_R8_SRGB:
    case TF_RG8:
    case TF_RG8_SRGB:
    case TF_RGB8:
    case TF_RGB8_SRGB:
    case TF_BGR8_SRGB:
    case TF_RGBA8:
    case TF_RGBA8_SRGB:
    case TF_R10G10B10A2:
    case TF_BGRA8:
    case TF_BGRA8_SRGB:
        return 1;
    case TF_R16:
    case TF_RG16:
    case TF_RGB16:
    case TF_RGBA16:
    case TF_DEPTH_16:
        return 2;
    case TF_R32:
    case TF_RG32:
    case TF_RGB32:
    case TF_RGBA32:
    case TF_R32_:
    case TF_RG16_:
    case TF_DEPTH_24:
    case TF_DEPTH_32F:
        return 4;
    case TF_R11G11B10F:
        return 1; // packed; treat as 1 component (1x4)
    case TF_R16F:
    case TF_RG16F:
    case TF_RGB16F:
    case TF_RGBA16F:
        return 2;
    case TF_R32F:
    case TF_RG32F:
    case TF_RGB32F:
    case TF_RGBA32F:
        return 4;
    default:
        return 0; // undefined result
    }
}

/*! \brief returns a texture format that has a shifted bytes-per-pixel count
 * e.g calling with RGB16 and num components = 4 --> RGBA16 */
static inline constexpr TextureFormat FormatChangeNumComponents(TextureFormat fmt, uint8 newNumComponents)
{
    if (newNumComponents == 0)
    {
        return TF_NONE;
    }

    newNumComponents = MathUtil::Clamp(newNumComponents, static_cast<uint8>(1), static_cast<uint8>(4));

    int currentNumComponents = int(NumComponents(fmt));

    return TextureFormat(int(fmt) + int(newNumComponents) - currentNumComponents);
}

static inline constexpr bool IsDepthFormat(TextureBaseFormat fmt)
{
    return fmt == TFB_DEPTH;
}

static inline constexpr bool IsDepthFormat(TextureFormat fmt)
{
    return IsDepthFormat(GetBaseFormat(fmt));
}

static inline constexpr bool HasStencilComponent(TextureFormat fmt)
{
    return fmt == TF_DEPTH_24; // assuming 8 bits of stencil in D24S8
}

static inline constexpr bool IsSrgbFormat(TextureFormat fmt)
{
    return fmt >= TF_SRGB && fmt < TF_DEPTH;
}

/*! \brief Converts srgb formats to non-srgb variants and vice versa. Only for SRGB supported formats. */
static inline constexpr TextureFormat ChangeFormatSrgb(TextureFormat fmt, bool makeSrgb = true)
{
    if (IsSrgbFormat(fmt) == makeSrgb)
    {
        return fmt;
    }

    constexpr uint32 dist = uint32(TF_SRGB) - uint32(TF_NONE);

    if (makeSrgb)
    {
        TextureFormat srgbVersion = static_cast<TextureFormat>(uint32(fmt) + dist);

        if (IsSrgbFormat(srgbVersion))
        {
            return srgbVersion;
        }
    }
    else
    {
        int iFmt = int(fmt);
        if (iFmt - int(dist) >= int(TF_NONE))
        {
            return static_cast<TextureFormat>(iFmt - int(dist));
        }
    }

    return fmt;
}

static inline constexpr bool FormatSupportsBlending(TextureFormat fmt)
{
    switch (fmt)
    {
    case TF_R8:
    case TF_R8_SRGB:
    case TF_RG8:
    case TF_RG8_SRGB:
    case TF_RGB8:
    case TF_RGB8_SRGB:
    case TF_BGR8_SRGB:
    case TF_RGBA8:
    case TF_RGBA8_SRGB:
    case TF_R11G11B10F:
    case TF_R10G10B10A2:
    case TF_BGRA8:
    case TF_BGRA8_SRGB:
    case TF_R16F:
    case TF_RG16F:
    case TF_RGB16F:
    case TF_RGBA16F:
    case TF_R32F:
    case TF_RG32F:
    case TF_RGB32F:
    case TF_RGBA32F:
        return true;
    case TF_R16:
    case TF_RG16:
    case TF_RGB16:
    case TF_RGBA16:
    case TF_R32:
    case TF_RG32:
    case TF_RGB32:
    case TF_RGBA32:
    case TF_R32_:
    case TF_RG16_:
    case TF_DEPTH_16:
    case TF_DEPTH_24:
    case TF_DEPTH_32F:
    default:
        return false;
    }
}

} // namespace TextureUtils

template <SizeType Size>
struct SizedUInt;

template <>
struct SizedUInt<1>
{
    using Type = uint8;
};
template <>
struct SizedUInt<2>
{
    using Type = uint16;
};
template <>
struct SizedUInt<4>
{
    using Type = uint32;
};
template <>
struct SizedUInt<8>
{
    using Type = uint64;
};

template <SizeType Size>
using SizedUIntT = typename SizedUInt<Size>::Type;

template <TextureFormat Format>
struct TextureFormatHelper
{
    static constexpr uint32 NumComponents = TextureUtils::NumComponents(Format);
    static constexpr uint32 BytesPerComponent = TextureUtils::BytesPerComponent(Format);
    static constexpr bool IsSrgb = TextureUtils::IsSrgbFormat(Format);
    static constexpr bool IsFloatType = uint32(Format) >= TF_R16F && uint32(Format) <= TF_RGBA32F;

    using ElementType = std::conditional_t<
        IsFloatType,
        std::conditional_t<(uint32(Format) <= TF_RGBA16F), Float16, float>,
        SizedUIntT<BytesPerComponent>>;
};

HYP_STRUCT()
struct TextureDesc
{
    HYP_STRUCT_BODY(TextureDesc);

    static constexpr uint32 MaxMips = 16;

    HYP_FIELD(Property = "Type", Serialize)
    TextureType type = TT_TEX2D;

    HYP_FIELD(Property = "Format", Serialize)
    TextureFormat format = TF_RGBA8;

    HYP_FIELD(Property = "Extent", Serialize)
    Vec3u extent = Vec3u::One();

    HYP_FIELD(Property = "MinFilterMode", Serialize)
    TextureFilterMode filterModeMin = TFM_NEAREST;

    HYP_FIELD(Property = "MagFilterMode", Serialize)
    TextureFilterMode filterModeMag = TFM_NEAREST;

    HYP_FIELD(Property = "TextureWrapMode", Serialize)
    TextureWrapMode wrapMode = TWM_CLAMP_TO_EDGE;

    HYP_FIELD(Property = "NumLayers", Serialize)
    uint32 numLayers = 1;

    HYP_FIELD(Property = "ImageUsage", Serialize)
    EnumFlags<ImageUsage> imageUsage = IU_SAMPLED;

    HYP_FIELD(Property = "MipOffsets", Serialize)
    FixedArray<uint32, MaxMips> mipOffsets = { 0 }; // first elem is the size of mip 0 (and offset of mip 1)

    HYP_FORCE_INLINE bool operator==(const TextureDesc& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const TextureDesc& other) const = default;

    HYP_FORCE_INLINE bool HasMipMaps() const
    {
        return filterModeMin == TFM_NEAREST_MIPMAP
            || filterModeMin == TFM_LINEAR_MIPMAP
            || filterModeMin == TFM_MINMAX_MIPMAP;
    }

    uint32 NumMips() const
    {
        return HasMipMaps()
            ? MathUtil::Min(MaxMips, uint32(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y, extent.z))) + 1)
            : 1;
    }

    HYP_FORCE_INLINE bool HasStoredMips() const
    {
        return mipOffsets[0] != 0;
    }

    HYP_FORCE_INLINE bool IsDepthStencil() const
    {
        return TextureUtils::IsDepthFormat(format);
    }

    HYP_FORCE_INLINE bool IsSrgb() const
    {
        return TextureUtils::IsSrgbFormat(format);
    }

    HYP_FORCE_INLINE bool IsBlended() const
    {
        return imageUsage[IU_BLENDED];
    }

    HYP_FORCE_INLINE bool IsTextureCube() const
    {
        return type == TT_CUBEMAP;
    }

    HYP_FORCE_INLINE bool IsPanorama() const
    {
        return type == TT_TEX2D
            && extent.x == extent.y * 2
            && extent.z == 1;
    }

    HYP_FORCE_INLINE bool IsTexture2DArray() const
    {
        return type == TT_TEX2D_ARRAY;
    }

    HYP_FORCE_INLINE bool IsTextureCubeArray() const
    {
        return type == TT_CUBEMAP_ARRAY;
    }

    HYP_FORCE_INLINE bool IsTexture3D() const
    {
        return type == TT_TEX3D;
    }

    HYP_FORCE_INLINE bool IsTexture2D() const
    {
        return type == TT_TEX2D;
    }

    uint32 NumArrayLayers() const
    {
        if (IsTextureCube() || IsTextureCubeArray())
        {
            return 6 * numLayers;
        }

        return numLayers;
    }

    Vec3u GetMipExtent(uint8 mipIndex) const
    {
        const uint32 numMips = NumMips();
        if (mipIndex >= numMips)
        {
            return Vec3u::Zero();
        }

        return Vec3u(
            MathUtil::Max(extent.x >> mipIndex, 1u),
            MathUtil::Max(extent.y >> mipIndex, 1u),
            MathUtil::Max(extent.z >> mipIndex, 1u));
    }

    uint32 GetByteSize() const
    {
        return uint32(extent.x * extent.y * extent.z)
            * TextureUtils::BytesPerComponent(format)
            * TextureUtils::NumComponents(format)
            * NumArrayLayers();
    }

    uint32 GetMipByteSize(uint8 mipIndex) const
    {
        const uint32 numMips = NumMips();
        if (mipIndex >= numMips)
        {
            return 0;
        }

        const Vec3u mipExtent = GetMipExtent(mipIndex);

        return uint32(mipExtent.x * mipExtent.y * mipExtent.z)
            * TextureUtils::BytesPerComponent(format)
            * TextureUtils::NumComponents(format);
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(format);
        hc.Add(extent);
        hc.Add(filterModeMin);
        hc.Add(filterModeMag);
        hc.Add(wrapMode);
        hc.Add(numLayers);
        hc.Add(mipOffsets);

        return hc;
    }
};

HYP_STRUCT()
struct TextureData
{
    HYP_STRUCT_BODY(TextureData);

    HYP_FIELD(Property = "ImageData", Serialize, Compressed)
    ByteBuffer imageData;

    TextureData() = default;

    explicit TextureData(const ByteBuffer& imageData)
        : imageData(imageData)
    {
    }

    explicit TextureData(ByteBuffer&& imageData)
        : imageData(std::move(imageData))
    {
    }

    TextureData(const TextureData& other) = default;
    TextureData& operator=(const TextureData& other) = default;

    TextureData(TextureData&& other) noexcept = default;
    TextureData& operator=(TextureData&& other) noexcept = default;

    ~TextureData() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return imageData.Any();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(imageData.GetHashCode());

        return hc;
    }
};

struct alignas(16) PackedVertex
{
    float positionX,
        positionY,
        positionZ,
        normalX,
        normalY,
        normalZ,
        texcoord0X,
        texcoord0Y;
};

static_assert(sizeof(PackedVertex) == sizeof(float32) * 8);

HYP_ENUM()
enum class GpuBufferType : uint8
{
    NONE = 0,
    MESH_INDEX_BUFFER,
    MESH_VERTEX_BUFFER,
    CBUFF,
    SSBO,
    ATOMIC_COUNTER,
    STAGING_BUFFER,
    INDIRECT_ARGS_BUFFER,
    SHADER_BINDING_TABLE,
    ACCELERATION_STRUCTURE_BUFFER,
    ACCELERATION_STRUCTURE_INSTANCE_BUFFER,
    RT_MESH_INDEX_BUFFER,
    RT_MESH_VERTEX_BUFFER,
    SCRATCH_BUFFER,
    MAX
};

HYP_ENUM()
enum GpuElemType : uint8
{
    GET_UNSIGNED_BYTE,
    GET_SIGNED_BYTE,
    GET_UNSIGNED_SHORT,
    GET_SIGNED_SHORT,
    GET_UNSIGNED_INT,
    GET_SIGNED_INT,
    GET_FLOAT,

    GET_MAX
};

static inline constexpr uint32 GpuElemTypeSize(GpuElemType type)
{
    constexpr uint32 sizes[GET_MAX] = {
        1, 1,
        2, 2,
        4, 4,
        4
    };

    return sizes[uint32(type)];
}

HYP_ENUM()
enum FaceCullMode : uint8
{
    FCM_NONE,
    FCM_BACK,
    FCM_FRONT
};

HYP_ENUM()
enum FillMode : uint8
{
    FM_FILL,
    FM_LINE
};

HYP_ENUM()
enum Topology : uint8
{
    TOP_TRIANGLES,
    TOP_TRIANGLE_FAN,
    TOP_TRIANGLE_STRIP,

    TOP_LINES,

    TOP_POINTS
};

HYP_ENUM()
enum BlendModeFactor : uint8
{
    BMF_NONE,

    BMF_ONE,
    BMF_ZERO,
    BMF_SRC_COLOR,
    BMF_SRC_ALPHA,
    BMF_DST_COLOR,
    BMF_DST_ALPHA,
    BMF_ONE_MINUS_SRC_COLOR,
    BMF_ONE_MINUS_SRC_ALPHA,
    BMF_ONE_MINUS_DST_COLOR,
    BMF_ONE_MINUS_DST_ALPHA,

    BMF_MAX
};

static_assert(uint32(BMF_MAX) <= 15, "BlendModeFactor enum too large to fit in 4 bits");

HYP_STRUCT(Serialize = "bitwise", Size = 4)
struct BlendFunction
{
    HYP_STRUCT_BODY(BlendFunction);

    uint32 value;

    BlendFunction()
        : BlendFunction(BMF_ONE, BMF_ZERO)
    {
    }

    BlendFunction(BlendModeFactor src, BlendModeFactor dst)
        : value((uint32(src) << 0) | (uint32(dst) << 4) | (uint32(src) << 8) | (uint32(dst) << 12))
    {
    }

    BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor, BlendModeFactor srcAlpha, BlendModeFactor dstAlpha)
        : value((uint32(srcColor) << 0) | (uint32(dstColor) << 4) | (uint32(srcAlpha) << 8) | (uint32(dstAlpha) << 12))
    {
    }

    BlendFunction(const BlendFunction& other) = default;
    BlendFunction& operator=(const BlendFunction& other) = default;
    BlendFunction(BlendFunction&& other) noexcept = default;
    BlendFunction& operator=(BlendFunction&& other) noexcept = default;
    ~BlendFunction() = default;

    HYP_FORCE_INLINE BlendModeFactor GetSrcColor() const
    {
        return BlendModeFactor(value & 0xF);
    }

    HYP_FORCE_INLINE void SetSrcColor(BlendModeFactor src)
    {
        value |= uint32(src);
    }

    HYP_FORCE_INLINE BlendModeFactor GetDstColor() const
    {
        return BlendModeFactor((value >> 4) & 0xF);
    }

    HYP_FORCE_INLINE void SetDstColor(BlendModeFactor dst)
    {
        value |= uint32(dst) << 4;
    }

    HYP_FORCE_INLINE BlendModeFactor GetSrcAlpha() const
    {
        return BlendModeFactor((value >> 8) & 0xF);
    }

    HYP_FORCE_INLINE void SetSrcAlpha(BlendModeFactor src)
    {
        value |= uint32(src) << 8;
    }

    HYP_FORCE_INLINE BlendModeFactor GetDstAlpha() const
    {
        return BlendModeFactor((value >> 12) & 0xF);
    }

    HYP_FORCE_INLINE void SetDstAlpha(BlendModeFactor dst)
    {
        value |= uint32(dst) << 12;
    }

    HYP_FORCE_INLINE bool operator==(const BlendFunction& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const BlendFunction& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE bool operator<(const BlendFunction& other) const
    {
        return value < other.value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }

    HYP_FORCE_INLINE static BlendFunction None()
    {
        return BlendFunction(BMF_NONE, BMF_NONE);
    }

    HYP_FORCE_INLINE static BlendFunction Default()
    {
        return BlendFunction(BMF_ONE, BMF_ZERO);
    }

    HYP_FORCE_INLINE static BlendFunction AlphaBlending()
    {
        return BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ZERO);
    }

    HYP_FORCE_INLINE static BlendFunction Additive()
    {
        return BlendFunction(BMF_ONE, BMF_ONE);
    }
};

HYP_ENUM()
enum RenderTargetType : uint8
{
    RTT_NONE = 0,
    RTT_PRESENT,         /* for presentation on screen */
    RTT_SHADER_RESOURCE, /* for use as a texture, sampled within in a shader */
    RTT_MAX
};

HYP_ENUM()
enum class LoadOperation : uint8
{
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

HYP_ENUM()
enum class StoreOperation : uint8
{
    UNDEFINED,
    NONE,
    STORE
};

HYP_ENUM()
enum StencilCompareOp : uint8
{
    SCO_ALWAYS,
    SCO_NEVER,
    SCO_EQUAL,
    SCO_NOT_EQUAL
};

HYP_ENUM()
enum StencilOp : uint8
{
    SO_KEEP,
    SO_ZERO,
    SO_REPLACE,
    SO_INCREMENT,
    SO_DECREMENT
};

HYP_STRUCT()
struct StencilFunction
{
    HYP_STRUCT_BODY(StencilFunction);

    HYP_FIELD(Serialize)
    StencilOp passOp = SO_REPLACE;

    HYP_FIELD(Serialize)
    StencilOp failOp = SO_KEEP;

    HYP_FIELD(Serialize)
    StencilOp depthFailOp = SO_KEEP;

    HYP_FIELD(Serialize)
    StencilCompareOp compareOp = SCO_ALWAYS;

    HYP_FORCE_INLINE bool operator==(const StencilFunction& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const StencilFunction& other) const = default;

    HYP_FORCE_INLINE bool operator<(const StencilFunction& other) const
    {
        return Memory::Compare(this, &other, sizeof(StencilFunction)) < 0;
    }

    HYP_FORCE_INLINE bool IsSet() const
    {
        return compareOp != SCO_ALWAYS
            || passOp != SO_KEEP
            || failOp != SO_KEEP
            || depthFailOp != SO_KEEP;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(passOp);
        hc.Add(failOp);
        hc.Add(depthFailOp);
        hc.Add(compareOp);

        return hc;
    }
};

struct PushConstantData
{
    ubyte data[128];
    uint32 size;

    PushConstantData()
        : data { 0 },
          size(0)
    {
    }

    PushConstantData(const void* ptr, uint32 size)
        : size(size)
    {
        Assert(size <= 128, "Push constant data size exceeds 128 bytes");

        Memory::Copy(&data[0], ptr, size);
    }

    template <class T>
    PushConstantData(const T* value)
        : size(uint32(sizeof(T)))
    {
        static_assert(sizeof(T) <= 128, "Push constant data size exceeds 128 bytes");
        static_assert(std::is_trivial_v<T>, "T must be a trivial type");
        static_assert(std::is_standard_layout_v<T>, "T must be a standard layout type");

        Memory::Copy(&data[0], value, sizeof(T));
    }

    PushConstantData(const PushConstantData& other) = default;
    PushConstantData& operator=(const PushConstantData& other) = default;
    PushConstantData(PushConstantData&& other) noexcept = default;
    PushConstantData& operator=(PushConstantData&& other) noexcept = default;
    ~PushConstantData() = default;

    HYP_FORCE_INLINE const void* Data() const
    {
        return data;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return size;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return size != 0;
    }
};

} // namespace Hyperion

#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Mat4f.hpp>

namespace Hyperion {
struct alignas(16) MeshDescription
{
    uint64 vertexBufferAddress;
    uint64 indexBufferAddress;

    uint32 _pad0;
    uint32 materialIndex;
    uint32 numIndices;
    uint32 numVertices;
};

static inline uint64 GetImageSubResourceKey(uint32 baseArrayLayer, uint32 baseMipLevel)
{
    return (uint64(baseArrayLayer) << 32) | (uint64(baseMipLevel));
}

/* images */
struct ImageSubResource
{
    uint32 baseArrayLayer = 0;
    uint32 baseMipLevel = 0;
    uint32 numLayers = 1;
    uint32 numLevels = 1;

    bool operator==(const ImageSubResource& other) const
    {
        return baseArrayLayer == other.baseArrayLayer
            && numLayers == other.numLayers
            && baseMipLevel == other.baseMipLevel
            && numLevels == other.numLevels;
    }

    uint64 GetSubResourceKey() const
    {
        return GetImageSubResourceKey(baseArrayLayer, baseMipLevel);
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(baseArrayLayer);
        hc.Add(numLayers);
        hc.Add(baseMipLevel);
        hc.Add(numLevels);

        return hc;
    }
};

HYP_STRUCT()
struct Viewport
{
    HYP_STRUCT_BODY(Viewport);

    HYP_FIELD()
    Vec2u extent;

    HYP_FIELD()
    Vec2i position;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return position != Vec2i::Zero() || extent != Vec2u::Zero();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return position == Vec2i::Zero() && extent == Vec2u::Zero();
    }

    HYP_FORCE_INLINE bool operator==(const Viewport& other) const
    {
        return position == other.position && extent == other.extent;
    }

    HYP_FORCE_INLINE bool operator!=(const Viewport& other) const
    {
        return position != other.position || extent != other.extent;
    }
};

HYP_ENUM()
enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    SSBO,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

HYP_STRUCT()
struct DescriptorSetOffsetMap
{
    HYP_STRUCT_BODY(DescriptorSetOffsetMap);

    static constexpr uint32 MaxOffsets = 16;

    StringHash keys[MaxOffsets];
    uint32 values[MaxOffsets];
    uint32 count;

    DescriptorSetOffsetMap()
        : keys(),
          values(),
          count(0)
    {
    }

    DescriptorSetOffsetMap(std::initializer_list<Pair<StringHash, uint32>> v)
        : keys(),
          values(),
          count(v.size())
    {
        AssertDebug(v.size() <= MaxOffsets, "too many values provided to constructor!");

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            keys[it - v.begin()] = it->first;
            values[it - v.begin()] = it->second;
        }
    }

    template <SizeType Count>
    DescriptorSetOffsetMap(Pair<StringHash, uint32> const (&kv)[Count])
        : keys(),
          values(),
          count(Count)
    {
        static_assert(Count <= MaxOffsets, "too many values provided to constructor!");

        for (uint32 i = 0; i < Count; i++)
        {
            keys[i] = kv[i].first;
            values[i] = kv[i].second;
        }
    }

    HYP_FORCE_INLINE void Add(StringHash key, uint32 value)
    {
        uint32 idx = count++;
        AssertDebug(idx < MaxOffsets, "too many offsets!");

        keys[idx] = key;
        values[idx] = value;
    }
};

HYP_STRUCT()
struct DescriptorTableOffsetMap
{
    HYP_STRUCT_BODY(DescriptorTableOffsetMap);

    static constexpr uint32 MaxSets = 4;

    StringHash setNames[MaxSets];
    DescriptorSetOffsetMap setOffsets[MaxSets];
    uint32 count;

    DescriptorTableOffsetMap()
        : setNames(),
          setOffsets(),
          count(0)
    {
    }

    DescriptorTableOffsetMap(std::initializer_list<Pair<StringHash, DescriptorSetOffsetMap>> v)
        : setNames(),
          setOffsets(),
          count(v.size())
    {
        AssertDebug(v.size() <= MaxSets, "too many values provided to constructor!");

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            setNames[it - v.begin()] = it->first;
            setOffsets[it - v.begin()] = it->second;
        }
    }

    template <SizeType Count>
    DescriptorTableOffsetMap(Pair<StringHash, DescriptorSetOffsetMap> const (&v)[Count])
        : setNames(),
          setOffsets(),
          count(Count)
    {
        static_assert(Count <= MaxSets, "too many values provided to constructor!");

        for (uint32 i = 0; i < Count; i++)
        {
            setNames[i] = v[i].first;
            setOffsets[i] = v[i].second;
        }
    }

    HYP_FORCE_INLINE DescriptorSetOffsetMap& Add(StringHash setName)
    {
        uint32 idx = count++;
        AssertDebug(idx < MaxSets, "too many offsets!");

        setNames[idx] = setName;

        return setOffsets[count];
    }
};

struct AttachmentDesc
{
    TextureType imageType = TT_TEX2D;
    TextureFormat format = TF_RGBA8;
    LoadOperation loadOp = LoadOperation::CLEAR;
    StoreOperation storeOp = StoreOperation::STORE;
    BlendFunction blendFunction = BlendFunction::None();
    float clearColor[4] = {};

    HYP_FORCE_INLINE bool operator==(const AttachmentDesc& other) const
    {
        return imageType == other.imageType
            && format == other.format
            && loadOp == other.loadOp
            && storeOp == other.storeOp
            && blendFunction == other.blendFunction
            && Memory::Compare((void*)clearColor, (const void*)other.clearColor, sizeof(clearColor)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const AttachmentDesc& other) const
    {
        return imageType != other.imageType
            || format != other.format
            || loadOp != other.loadOp
            || storeOp != other.storeOp
            || blendFunction != other.blendFunction
            || Memory::Compare((void*)clearColor, (const void*)other.clearColor, sizeof(clearColor)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(uint32(imageType));
        hc.Add(uint32(format));
        hc.Add(uint8(loadOp));
        hc.Add(uint8(storeOp));
        hc.Add(blendFunction);
        hc.Add(clearColor);

        return hc;
    }
};

struct RenderTargetDesc
{
    static constexpr uint32 MaxAttachments = 5;
    
    Vec2u extent = Vec2u::One();
    
    uint32 numAttachments = 0;
    AttachmentDesc attachments[MaxAttachments];

    uint32 numLayers = 1;

    HYP_FORCE_INLINE bool operator==(const RenderTargetDesc& other) const
    {
        return extent == other.extent
            && numAttachments == other.numAttachments
            && std::equal(attachments, attachments + numAttachments, other.attachments)
            && numLayers == other.numLayers;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderTargetDesc& other) const
    {
        return !(*this == other);
    }

    void AddAttachment(const AttachmentDesc& attachmentDesc)
    {
        if (numAttachments >= MaxAttachments)
            return;

        attachments[numAttachments++] = attachmentDesc;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(extent)
            .Combine(numAttachments)
            .Combine(HashCode::GetHashCode(attachments, attachments + numAttachments))
            .Combine(numLayers);
    }
};

enum class VertexAttributeName : uint8
{
    Undefined,
    Position,
    Normal,
    TexCoord0,
    TexCoord1,
    Tangent,
    Bitangent,
    BoneIndices,
    BoneWeights
};

/*! \brief Represents a vertex attribute used in mesh input.
 *  \details This struct defines the properties of a vertex attribute, including its name, location, binding, and size.
 *  It is used to describe the layout of vertex data in a mesh and is essential for rendering operations. */
struct VertexAttribute
{
    static const VertexAttribute Position;
    static const VertexAttribute Normal;
    static const VertexAttribute TexCoord0;
    static const VertexAttribute TexCoord1;
    static const VertexAttribute Tangent;
    static const VertexAttribute Bitangent;
    static const VertexAttribute BoneIndices;
    static const VertexAttribute BoneWeights;

    static const VertexAttribute* Attrs[];

    const char* name;
    uint32 location;
    uint32 binding;
    uint32 size; // total size == num elements * 4

    HYP_FORCE_INLINE bool operator<(const VertexAttribute& other) const
    {
        return location < other.location;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(location);
        hc.Add(binding);
        hc.Add(size);

        return hc;
    }
};

inline const VertexAttribute VertexAttribute::Position = { "a_position", 0, 0, 3 * sizeof(float) };
inline const VertexAttribute VertexAttribute::Normal = { "a_normal", 1, 0, 3 * sizeof(float) };
inline const VertexAttribute VertexAttribute::TexCoord0 = { "a_texcoord0", 2, 0, 2 * sizeof(float) };
inline const VertexAttribute VertexAttribute::TexCoord1 = { "a_texcoord1", 3, 0, 2 * sizeof(float) };
inline const VertexAttribute VertexAttribute::Tangent = { "a_tangent", 4, 0, 3 * sizeof(float) };
inline const VertexAttribute VertexAttribute::Bitangent = { "a_bitangent", 5, 0, 3 * sizeof(float) };
inline const VertexAttribute VertexAttribute::BoneIndices = { "a_bone_weights", 6, 0, 4 * sizeof(float) };
inline const VertexAttribute VertexAttribute::BoneWeights { "a_bone_indices", 7, 0, 4 * sizeof(float) };

/*! \brief Represents a set of vertex attributes used in mesh input.
 *  \details This struct is a bitmask representation of vertex attributes, allowing for efficient storage and manipulation of vertex attribute flags.
 *  It provides methods for checking, setting, and merging vertex attributes, as well as calculating the size of the vertex data based on the attributes. */
HYP_STRUCT(Serialize = "bitwise")
struct VertexAttributeSet
{
    HYP_STRUCT_BODY(VertexAttributeSet);
    
    static const VertexAttributeSet StaticMeshVertexAttributes;
    static const VertexAttributeSet SkeletalMeshVertexAttributes;

    HYP_FIELD()
    uint64 flagMask;

    constexpr VertexAttributeSet()
        : flagMask(0)
    {
    }

    constexpr VertexAttributeSet(uint64 flagMask)
        : flagMask(flagMask)
    {
    }

    constexpr VertexAttributeSet(const VertexAttributeSet& other) = default;
    VertexAttributeSet& operator=(const VertexAttributeSet& other) = default;

    ~VertexAttributeSet() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return flagMask != 0;
    }

    HYP_FORCE_INLINE bool operator==(const VertexAttributeSet& other) const
    {
        return flagMask == other.flagMask;
    }

    HYP_FORCE_INLINE bool operator!=(const VertexAttributeSet& other) const
    {
        return flagMask != other.flagMask;
    }

    HYP_FORCE_INLINE bool operator==(uint64 flags) const
    {
        return flagMask == flags;
    }

    HYP_FORCE_INLINE bool operator!=(uint64 flags) const
    {
        return flagMask != flags;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator~() const
    {
        return ~flagMask;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator&(const VertexAttributeSet& other) const
    {
        return { flagMask & other.flagMask };
    }

    HYP_FORCE_INLINE VertexAttributeSet& operator&=(const VertexAttributeSet& other)
    {
        flagMask &= other.flagMask;
        return *this;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator&(uint64 flags) const
    {
        return { flagMask & flags };
    }

    HYP_FORCE_INLINE VertexAttributeSet& operator&=(uint64 flags)
    {
        flagMask &= flags;
        return *this;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator|(const VertexAttributeSet& other) const
    {
        return { flagMask | other.flagMask };
    }

    HYP_FORCE_INLINE VertexAttributeSet& operator|=(const VertexAttributeSet& other)
    {
        flagMask |= other.flagMask;
        return *this;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator|(const VertexAttribute& attr) const
    {
        return { flagMask | (1ull << attr.location) };
    }

    HYP_FORCE_INLINE VertexAttributeSet& operator|=(const VertexAttribute& attr)
    {
        flagMask |= (1ull << attr.location);
        return *this;
    }

    HYP_FORCE_INLINE VertexAttributeSet operator|(uint64 flags) const
    {
        return { flagMask | flags };
    }

    HYP_FORCE_INLINE VertexAttributeSet& operator|=(uint64 flags)
    {
        flagMask |= flags;
        return *this;
    }

    HYP_FORCE_INLINE bool operator<(const VertexAttributeSet& other) const
    {
        return flagMask < other.flagMask;
    }

    HYP_FORCE_INLINE bool Has(const VertexAttribute& attr) const
    {
        return bool(*this & (1ull << attr.location));
    }

    HYP_FORCE_INLINE void Set(uint64 flags, bool enable = true)
    {
        if (enable)
        {
            flagMask |= flags;
        }
        else
        {
            flagMask &= ~flags;
        }
    }

    HYP_FORCE_INLINE void Set(const VertexAttribute& attr, bool enable = true)
    {
        Set(1ull << attr.location, enable);
    }

    HYP_FORCE_INLINE void Merge(const VertexAttributeSet& other)
    {
        flagMask |= other.flagMask;
    }

    HYP_FORCE_INLINE uint64 GetFlagMask() const
    {
        return flagMask;
    }

    HYP_FORCE_INLINE void SetFlagMask(uint64 flags)
    {
        flagMask = flags;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return uint32(ByteUtil::BitCount(flagMask));
    }

    HYP_API Array<const VertexAttribute*> BuildAttributes() const;
    HYP_API SizeType CalculateVertexSize() const;

    HYP_API String ToString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flagMask);

        return hc;
    }
};

inline VertexAttributeSet operator|(const VertexAttribute& lhs, const VertexAttribute& rhs)
{
    return { (1ull << lhs.location) | (1ull << rhs.location) };
}

inline VertexAttributeSet operator&(const VertexAttribute& lhs, const VertexAttribute& rhs)
{
    return { (1ull << lhs.location) & (1ull << rhs.location) };
}

inline VertexAttributeSet operator~(const VertexAttribute& attr)
{
    return { ~(1ull << attr.location) };
}

HYP_STRUCT()
struct VertexAttributeDefinition
{
    HYP_STRUCT_BODY(VertexAttributeDefinition);

    HYP_FIELD()
    String name;

    HYP_FIELD()
    String typeClass;

    HYP_FIELD()
    int location = -1;

    HYP_FIELD()
    String condition;
};

HYP_ENUM()
enum ShaderPropertyFlags : uint8
{
    SPF_NONE = 0x0,
    SPF_VERTEX_ATTRIBUTE = 0x1,
    SPF_PERMUTATION = 0x2
};

HYP_STRUCT()
struct ShaderProperty
{
    HYP_STRUCT_BODY(ShaderProperty);

    using Value = Variant<Name, int, float>;

    HYP_FIELD(Property = "Name")
    Name name;

    HYP_FIELD(Property = "Flags")
    ShaderPropertyFlags flags;

    HYP_FIELD(Transient)
    Value currentValue;

    HYP_FIELD(Transient)
    Array<Value> enumValues;

    HYP_FIELD(Transient)
    HashCode cachedHashCode;

    ShaderProperty()
        : flags(SPF_NONE)
    {
    }

    explicit ShaderProperty(Name name, ShaderPropertyFlags flags = SPF_NONE)
        : name(name),
          flags(flags)
    {
    }

    ShaderProperty(Name name, const Value& currentValue, ShaderPropertyFlags flags = SPF_NONE)
        : name(name),
          flags(flags),
          currentValue(currentValue)
    {
    }

    explicit ShaderProperty(const VertexAttribute& vertexAttribute)
        : name(CreateNameFromDynamicString(ANSIString("HYP_ATTRIBUTE_") + vertexAttribute.name)),
          flags(SPF_VERTEX_ATTRIBUTE),
          currentValue(Value(CreateNameFromDynamicString(vertexAttribute.name)))
    {
        cachedHashCode = GetHashCode();
    }

    ShaderProperty(const ShaderProperty& other)
        : name(other.name),
          flags(other.flags),
          currentValue(other.currentValue),
          enumValues(other.enumValues),
          cachedHashCode(other.cachedHashCode)
    {
    }

    ShaderProperty& operator=(const ShaderProperty& other)
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        flags = other.flags;
        currentValue = other.currentValue;
        enumValues = other.enumValues;
        cachedHashCode = other.cachedHashCode;

        return *this;
    }

    ShaderProperty(ShaderProperty&& other) noexcept
        : name(other.name),
          flags(other.flags),
          currentValue(std::move(other.currentValue)),
          enumValues(std::move(other.enumValues)),
          cachedHashCode(other.cachedHashCode)
    {
        other.name = Name();
        other.flags = SPF_NONE;
        other.cachedHashCode = HashCode();
    }

    ShaderProperty& operator=(ShaderProperty&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        flags = other.flags;
        currentValue = std::move(other.currentValue);
        enumValues = std::move(other.enumValues);
        cachedHashCode = other.cachedHashCode;

        other.name = Name();
        other.flags = SPF_NONE;
        other.cachedHashCode = HashCode();

        return *this;
    }

    HYP_FORCE_INLINE bool operator==(const ShaderProperty& other) const
    {
        return name == other.name;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderProperty& other) const
    {
        return name != other.name;
    }

    // HYP_FORCE_INLINE bool operator==(const ShaderProperty& other) const
    // {
    //     return cachedHashCode == other.cachedHashCode
    //         // enum values are not included in hash code
    //         && (!IsValueGroup() || enumValues == other.enumValues);
    // }

    // HYP_FORCE_INLINE bool operator!=(const ShaderProperty& other) const
    // {
    //     return !(*this == other);
    // }

    HYP_FORCE_INLINE bool operator<(const ShaderProperty& other) const
    {
        if (name == other.name)
        {
            return false;
        }

        return std::strcmp(*name, *other.name) < 0;
    }

    HYP_FORCE_INLINE bool IsValueGroup() const
    {
        return enumValues.Any();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return currentValue.HasValue();
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE ShaderPropertyFlags GetFlags() const
    {
        return flags;
    }

    HYP_FORCE_INLINE bool IsPermutable() const
    {
        return flags & SPF_PERMUTATION;
    }

    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !IsPermutable() && !IsValueGroup();
    }

    HYP_FORCE_INLINE bool IsVertexAttribute() const
    {
        return flags & SPF_VERTEX_ATTRIBUTE;
    }

    HYP_FORCE_INLINE bool IsOptionalVertexAttribute() const
    {
        return IsVertexAttribute() && IsPermutable();
    }

    HYP_FORCE_INLINE void AddEnumValue(const Value& enumValue)
    {
        if (!enumValues.Contains(enumValue))
        {
            enumValues.PushBack(enumValue);
        }
    }

    HYP_API String GetValueString() const;

    HYP_API HashCode GetHashCode() const;

    HYP_API String ToString() const;
};

HYP_STRUCT()
class ShaderProperties
{
    friend class ShaderCompiler;

public:
    HYP_STRUCT_BODY(ShaderProperties);

    using Iterator = typename HashSet<ShaderProperty>::Iterator;
    using ConstIterator = typename HashSet<ShaderProperty>::ConstIterator;

    ShaderProperties()
        : m_needsHashCodeRecalculation(true)
    {
    }

    explicit ShaderProperties(const HashSet<ShaderProperty>& props)
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property, true);
        }
    }

    template <SizeType Sz>
    ShaderProperties(Name const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            Set(ShaderProperty(propKey, SPF_PERMUTATION), true); // default to permutable
        }
    }

    template <SizeType Sz>
    ShaderProperties(ShaderProperty const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property, true);
        }
    }

    template <SizeType Sz>
    ShaderProperties(const VertexAttributeSet& vertexAttributes, Name const (&props)[Sz])
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            m_props.Insert(ShaderProperty(propKey, SPF_PERMUTATION)); // default to permutable
        }
    }

    explicit ShaderProperties(const VertexAttributeSet& vertexAttributes)
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
    }

    ShaderProperties(const ShaderProperties& other) = default;
    ShaderProperties& operator=(const ShaderProperties& other) = default;

    ShaderProperties(ShaderProperties&& other) noexcept = default;
    ShaderProperties& operator=(ShaderProperties&& other) = default;

    ~ShaderProperties() = default;

    // HYP_FORCE_INLINE bool operator==(const ShaderProperties& other) const
    // {
    //     return (m_requiredVertexAttributes == other.m_requiredVertexAttributes) && (m_props == other.m_props);
    // }

    // HYP_FORCE_INLINE bool operator!=(const ShaderProperties& other) const
    // {
    //     return m_requiredVertexAttributes != other.m_requiredVertexAttributes || m_props != other.m_props;
    // }

    HYP_FORCE_INLINE bool operator==(const ShaderProperties& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ShaderProperties& other) const = delete;

    HYP_FORCE_INLINE bool Any() const
    {
        return m_props.Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_props.Empty();
    }

    HYP_FORCE_INLINE Iterator Find(const ShaderProperty& property)
    {
        return m_props.Find(property);
    }

    HYP_FORCE_INLINE ConstIterator Find(const ShaderProperty& property) const
    {
        return const_cast<ShaderProperties*>(this)->Find(property);
    }

    Iterator Find(StringHash name)
    {
        const HashCode hashCode = name.GetHashCode();

        auto firstResultIt = m_props.FindByHashCode(hashCode);
        if (firstResultIt != m_props.End())
        {
            return firstResultIt;
        }

        // Do a full search if not found by hash code match (e.g. ShaderProperty has a value assigned)
        for (auto it = m_props.Begin(); it != m_props.End(); ++it)
        {
            if (it->name == name)
            {
                return it;
            }
        }

        return m_props.End();
    }

    HYP_FORCE_INLINE ConstIterator Find(StringHash name) const
    {
        return const_cast<ShaderProperties*>(this)->Find(name);
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttributes(VertexAttributeSet vertexAttributes) const
    {
        return (m_requiredVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttribute(const VertexAttribute& vertexAttribute) const
    {
        return m_requiredVertexAttributes.Has(vertexAttribute);
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttributes(VertexAttributeSet vertexAttributes) const
    {
        return (m_optionalVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttribute(const VertexAttribute& vertexAttribute) const
    {
        return m_optionalVertexAttributes.Has(vertexAttribute);
    }

    HYP_FORCE_INLINE bool Has(const ShaderProperty& property) const
    {
        return Find(property) != m_props.End();
    }

    HYP_FORCE_INLINE bool Has(StringHash name) const
    {
        return Find(name) != m_props.End();
    }

    HYP_API ShaderProperties& Set(const ShaderProperty& property, bool enabled = true);

    HYP_FORCE_INLINE ShaderProperties& Set(Name name, bool enabled = true, ShaderPropertyFlags flags = SPF_NONE)
    {
        return Set(ShaderProperty(name, flags), enabled);
    }

    /*! \brief Applies \p other properties onto this set */
    void Merge(const ShaderProperties& other)
    {
        for (const ShaderProperty& property : other.m_props)
        {
            Set(property, true);
        }

        m_requiredVertexAttributes |= other.m_requiredVertexAttributes;
        m_optionalVertexAttributes |= other.m_optionalVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    static ShaderProperties Merge(const ShaderProperties& a, const ShaderProperties& b)
    {
        ShaderProperties result(a);
        result.Merge(b);

        return result;
    }

    HYP_FORCE_INLINE const HashSet<ShaderProperty>& GetPropertySet() const
    {
        return m_props;
    }

    /*! \brief Adds a new permutation shader property
     *  Permutations create new shader variants based on their values.
     *  Many permutations will drastically increase the number of shader variants generated,
     *  so use them sparingly. (prefer value groups or static properties where appropriate) */
    ShaderProperties& AddPermutation(Name key)
    {
        const ShaderProperty shaderProperty(key, SPF_PERMUTATION);

        const auto it = m_props.Find(shaderProperty);

        if (it == m_props.End())
        {
            m_props.Insert(shaderProperty);
        }
        else
        {
            *it = shaderProperty;
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    /*! \brief Adds a new static property with key \p key
     *  Static properties are applied to every shader variant and do not create new permutations. */
    ShaderProperties& AddStatic(Name key)
    {
        const ShaderProperty shaderProperty(key, SPF_NONE);

        const auto it = m_props.Find(shaderProperty);

        if (it == m_props.End())
        {
            m_props.Insert(shaderProperty);
        }
        else
        {
            *it = shaderProperty;
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    /*! \brief Adds a new value group property with key \p key and possible enum values \p enumValues
     *  Value groups create new shader variants but their values are mututally exclusive to each other.
     *  i.e, only one value from the value group can be selected at a time. This reduces the number of
     *  shader variants generated compared to permutations. */
    ShaderProperties& AddValueGroup(Name key, const Array<ShaderProperty::Value>& enumValues)
    {
        ShaderProperty shaderProperty(key, SPF_NONE);

        if (enumValues.Any())
        {
            shaderProperty.enumValues = enumValues;
        }

        const auto it = m_props.Find(shaderProperty);

        if (it == m_props.End())
        {
            m_props.Insert(std::move(shaderProperty));
        }
        else
        {
            *it = std::move(shaderProperty);
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetRequiredVertexAttributes() const
    {
        return m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetOptionalVertexAttributes() const
    {
        return m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetAllVertexAttributes() const
    {
        return m_requiredVertexAttributes | m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE void SetRequiredVertexAttributes(VertexAttributeSet vertexAttributes)
    {
        m_requiredVertexAttributes = vertexAttributes;
        m_optionalVertexAttributes = m_optionalVertexAttributes & ~m_requiredVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE void SetOptionalVertexAttributes(VertexAttributeSet vertexAttributes)
    {
        m_optionalVertexAttributes = vertexAttributes & ~m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_props.Size();
    }

    HYP_FORCE_INLINE Array<ShaderProperty> ToArray() const
    {
        return m_props.ToArray();
    }

    HYP_API String ToString() const;

    HashCode GetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedHashCode;
    }

    HashCode GetPropertySetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedPropertySetHashCode;
    }

    HYP_DEF_STL_BEGIN_END(m_props.Begin(), m_props.End());

private:
    void RecalculateHashCode() const
    {
        HashCode hc;

        // NOTE: Intentionally left out m_optionalVertexAttributes
        // as they do not impact the final instantiated version of the shader properties.
        // m_requiredVertexAttributes needs to be checked by the caller as we could have
        // shader with less vertex attributes than the mesh in question has.

        m_cachedPropertySetHashCode = HashCode();

        Array<const ShaderProperty*> propsPtrs;
        propsPtrs.Reserve(m_props.Size());

        for (const ShaderProperty& property : m_props)
        {
            propsPtrs.PushBack(&property);
        }

        std::sort(propsPtrs.Begin(), propsPtrs.End(), [](const ShaderProperty* a, const ShaderProperty* b)
            {
                // sort by name to ensure consistent hashcode
                return std::strcmp(*a->name, *b->name) < 0;
            });

        for (const ShaderProperty* pShaderProperty : propsPtrs)
        {
            m_cachedPropertySetHashCode.Add(pShaderProperty->GetHashCode());
        }

        hc.Add(m_cachedPropertySetHashCode);

        m_cachedHashCode = hc;
    }

    HYP_FIELD()
    HashSet<ShaderProperty> m_props;

    HYP_FIELD()
    VertexAttributeSet m_requiredVertexAttributes;

    HYP_FIELD()
    VertexAttributeSet m_optionalVertexAttributes;

    mutable HashCode m_cachedHashCode;
    mutable HashCode m_cachedPropertySetHashCode;
    mutable bool m_needsHashCodeRecalculation;
};

HYP_STRUCT()
struct HashedShaderDefinition
{
    HYP_STRUCT_BODY(HashedShaderDefinition);

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    HashCode propertySetHash;

    HYP_FIELD()
    VertexAttributeSet requiredVertexAttributes;

    HYP_FORCE_INLINE bool operator==(const HashedShaderDefinition& other) const
    {
        return name == other.name
            && propertySetHash == other.propertySetHash
            && requiredVertexAttributes == other.requiredVertexAttributes;
    }

    HYP_FORCE_INLINE bool operator!=(const HashedShaderDefinition& other) const
    {
        return name != other.name
            || propertySetHash != other.propertySetHash
            || requiredVertexAttributes != other.requiredVertexAttributes;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        hc.Add(requiredVertexAttributes.GetHashCode());
        hc.Add(propertySetHash);

        return hc;
    }
};

HYP_STRUCT()
struct ShaderDefinition
{
    HYP_STRUCT_BODY(ShaderDefinition);

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    ShaderProperties properties;

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE ShaderProperties& GetProperties()
    {
        return properties;
    }

    HYP_FORCE_INLINE const ShaderProperties& GetProperties() const
    {
        return properties;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const ShaderDefinition& other) const
    {
        return GetHashCode() == other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderDefinition& other) const
    {
        return GetHashCode() != other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator<(const ShaderDefinition& other) const
    {
        return GetHashCode() < other.GetHashCode();
    }

    HYP_FORCE_INLINE explicit operator HashedShaderDefinition() const
    {
        return HashedShaderDefinition { name, properties.GetPropertySetHashCode(), properties.GetRequiredVertexAttributes() };
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // ensure they return the same hash codes so they can be compared.
        return (operator HashedShaderDefinition()).GetHashCode();
    }
};

/*! \brief For requested shader instance */
struct ShaderDesc
{
    static constexpr uint32 MaxShaderProperties = 8;

    struct PropertyValue
    {
        union
        {
            Name nameValue;
            int intValue;
            float floatValue;
        };

        uint8 index;

        PropertyValue()
            : nameValue(),
              index(uint8(-1))
        {
        }

        PropertyValue(Name value)
            : nameValue(value),
              index(0)
        {
        }

        PropertyValue(int value)
            : intValue(value),
              index(1)
        {
        }

        PropertyValue(float value)
            : floatValue(value),
              index(2)
        {
        }

        PropertyValue(const PropertyValue& other) = default;
        PropertyValue& operator=(const PropertyValue& other) = default;

        bool operator==(const PropertyValue& other) const
        {
            if (index != other.index)
            {
                return false;
            }

            switch (index)
            {
            case 0:
                return nameValue == other.nameValue;
            case 1:
                return intValue == other.intValue;
            case 2:
                return floatValue == other.floatValue;
            }

            return false;
        }

        bool operator!=(const PropertyValue& other) const
        {
            return !(*this == other);
        }

        HYP_FORCE_INLINE bool IsValid() const
        {
            return index != uint8(-1);
        }

        HYP_FORCE_INLINE bool IsName() const
        {
            return index == 0;
        }

        HYP_FORCE_INLINE bool IsInt() const
        {
            return index == 1;
        }

        HYP_FORCE_INLINE bool IsFloat() const
        {
            return index == 2;
        }

        HYP_FORCE_INLINE Name GetName() const
        {
            return index == 0 ? nameValue : Name::Invalid();
        }

        HYP_FORCE_INLINE int GetInt() const
        {
            return index == 1 ? intValue : 0;
        }

        HYP_FORCE_INLINE float GetFloat() const
        {
            return index == 2 ? floatValue : 0.0f;
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            switch (index)
            {
            case 0:
                return nameValue.GetHashCode();
            case 1:
                return HashCode::GetHashCode(intValue);
            case 2:
                return HashCode::GetHashCode(floatValue);
            default:
                return HashCode();
            }
        }
    };

    Name shaderName;
    Name propertyNames[MaxShaderProperties];
    PropertyValue propertyValues[MaxShaderProperties];
    uint32 numProperties = 0;

    ShaderDesc() = default;
    
    explicit ShaderDesc(Name shaderName)
        : shaderName(shaderName),
          numProperties(0)
    {
    }

    template <SizeType N>
    ShaderDesc(Name shaderName, const Pair<Name, PropertyValue>(&properties)[N])
        : shaderName(shaderName),
          numProperties(N)
    {
        static_assert(N <= MaxShaderProperties);

        for (uint32 i = 0; i < N; i++)
        {
            propertyNames[i] = properties[i].first;
            propertyValues[i] = properties[i].second;
        }
    }

    explicit ShaderDesc(const ShaderDefinition& shaderDefinition)
        : shaderName(shaderDefinition.name),
          numProperties(0)
    {
        for (const ShaderProperty& property : shaderDefinition.GetProperties().GetPropertySet())
        {
            const uint32 propertyIndex = numProperties++;

            if (propertyIndex == ShaderDesc::MaxShaderProperties)
            {
                break;
            }

            propertyNames[propertyIndex] = property.name;
            if (property.HasValue())
            {
                if (property.currentValue.Is<Name>())
                    propertyValues[propertyIndex] = property.currentValue.GetUnchecked<Name>();
                else if (property.currentValue.Is<int>())
                    propertyValues[propertyIndex] = property.currentValue.GetUnchecked<int>();
                else if (property.currentValue.Is<float>())
                    propertyValues[propertyIndex] = property.currentValue.GetUnchecked<float>();
            }
        }
    }

    ShaderDesc(const ShaderDesc& other) = default;
    ShaderDesc& operator=(const ShaderDesc& other) = default;

    HYP_FORCE_INLINE bool operator==(const ShaderDesc& other) const
    {
        return shaderName == other.shaderName
            && numProperties == other.numProperties
            && std::equal(propertyNames, propertyNames + numProperties, other.propertyNames)
            && std::equal(propertyValues, propertyValues + numProperties, other.propertyValues);
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderDesc& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(shaderName)
            .Combine(numProperties)
            .Combine(HashCode::GetHashCode(propertyNames, propertyNames + numProperties))
            .Combine(HashCode::GetHashCode(propertyValues, propertyValues + numProperties));
    }
};

HYP_ENUM()
enum ShaderModuleType : uint8
{
    SMT_UNSET = 0,

    /* Graphics and general purpose shaders */
    SMT_VERTEX,
    SMT_FRAGMENT,
    SMT_GEOMETRY,
    SMT_COMPUTE,

    /* Mesh shaders */
    SMT_TASK,
    SMT_MESH,

    /* Tesselation */
    SMT_TESS_CONTROL,
    SMT_TESS_EVAL,

    /* RayTracing hardware specific */
    SMT_RAY_GEN,
    SMT_RAY_INTERSECT,
    SMT_RAY_ANY_HIT,
    SMT_RAY_CLOSEST_HIT,
    SMT_RAY_MISS,

    SMT_MAX
};

static constexpr inline bool IsRayTracingShaderModule(ShaderModuleType type)
{
    return type == SMT_RAY_GEN
        || type == SMT_RAY_INTERSECT
        || type == SMT_RAY_ANY_HIT
        || type == SMT_RAY_CLOSEST_HIT
        || type == SMT_RAY_MISS;
}

struct ShaderBundleDecl // combination of shader files, .frag, .vert etc. in .ini definitions file.
{
    Name name;
    String entryPointName = "main";
    FlatMap<ShaderModuleType, String> sources;
    ShaderProperties versions; // permutations

    bool HasRTShaders() const
    {
        return AnyOf(sources, [](const KeyValuePair<ShaderModuleType, String>& item)
            {
                return IsRayTracingShaderModule(item.first);
            });
    }

    bool IsComputeShader() const
    {
        return Every(sources, [](const KeyValuePair<ShaderModuleType, String>& item)
            {
                return item.first == SMT_COMPUTE;
            });
    }

    bool HasVertexShader() const
    {
        return AnyOf(sources, [](const KeyValuePair<ShaderModuleType, String>& item)
            {
                return item.first == SMT_VERTEX;
            });
    }
};

} // namespace Hyperion
