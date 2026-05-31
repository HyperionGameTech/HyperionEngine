/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Float16.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/Variant.hpp>

#include <Core/Containers/FlatMap.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Color.hpp>

#include <Rendering/Vertex.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Util/EnumOptions.hpp>

#ifdef HYP_VULKAN
#include <Rendering/Vulkan/VulkanStructs.hpp>
#elif defined(HYP_DX12)
#include <Rendering/DX12/DX12Structs.hpp>
#endif

namespace Hyperion {

class ObjectBase;

namespace Resources {
static constexpr uint32 InvalidBinding = ~0u;
extern uint32 GetBinding(const ObjectBase* resource);
} // namespace Resources

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
enum class ImageSupport : uint8
{
    Attachment,
    ShaderResource,
    UnorderedAccess
};

HYP_ENUM()
enum DefaultImageFormat : uint8
{
    DIF_NONE,
    DIF_COLOR,
    DIF_DEPTH
};

HYP_ENUM()
enum class TextureType : uint8
{
    Texture2D,
    Texture3D,
    Cubemap,
    Texture2DArray,
    CubemapArray,

    Max
};

static constexpr TextureType InvalidTextureType = TextureType(-1);

HYP_ENUM()
enum class TextureBaseFormat : uint8
{
    Red,
    RG,
    RGB,
    RGBA,

    BGR,
    BGRA,

    Depth,
    DepthStencil
};

HYP_ENUM()
enum class TextureFormat : uint8
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
};

static constexpr TextureFormat InvalidTextureFormat = TextureFormat(-1);

inline constexpr bool operator<(TextureFormat a, TextureFormat b)
{
    return uint8(a) < uint8(b);
}

inline constexpr bool operator<=(TextureFormat a, TextureFormat b)
{
    return uint8(a) <= uint8(b);
}

inline constexpr bool operator>(TextureFormat a, TextureFormat b)
{
    return uint8(a) > uint8(b);
}

inline constexpr bool operator>=(TextureFormat a, TextureFormat b)
{
    return uint8(a) >= uint8(b);
}

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

static inline constexpr uint32 NumComponents(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8:
    case TextureFormat::R8_SRGB:
    case TextureFormat::R16:
    case TextureFormat::R32:
    case TextureFormat::R16F:
    case TextureFormat::R32F:
        return 1;
    case TextureFormat::RG8:
    case TextureFormat::RG8_SRGB:
    case TextureFormat::RG16:
    case TextureFormat::RG32:
    case TextureFormat::RG16F:
    case TextureFormat::RG32F:
        return 2;
    case TextureFormat::RGB8:
    case TextureFormat::RGB8_SRGB:
    case TextureFormat::RGB16:
    case TextureFormat::RGB32:
    case TextureFormat::RGB16F:
    case TextureFormat::RGB32F:
        return 3;
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::R11G11B10F: // treat R11G11B10F as RGBA so it is correctly calculated as 4 bytes per pixel.
    case TextureFormat::R10G10B10A2:  // same as above
    case TextureFormat::RGBA16:
    case TextureFormat::RGBA32:
    case TextureFormat::RGBA16F:
    case TextureFormat::RGBA32F:
        return 4;
    case TextureFormat::BGR8_SRGB:
        return 3;
    case TextureFormat::BGRA8:
    case TextureFormat::BGRA8_SRGB:
        return 4;
    case TextureFormat::D16:
    case TextureFormat::D24_S8:
    case TextureFormat::D32F:
        return 1;
    case TextureFormat::D32F_S8:
        return 2;
    default:
        // undefined result
        return 0;
    }
}

static inline constexpr uint32 BytesPerComponent(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::R8:
    case TextureFormat::R8_SRGB:
    case TextureFormat::RG8:
    case TextureFormat::RG8_SRGB:
    case TextureFormat::RGB8:
    case TextureFormat::RGB8_SRGB:
    case TextureFormat::BGR8_SRGB:
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::BGRA8:
    case TextureFormat::BGRA8_SRGB:
        return 1;
    case TextureFormat::R16:
    case TextureFormat::RG16:
    case TextureFormat::RGB16:
    case TextureFormat::RGBA16:
    case TextureFormat::D16:
        return 2;
    case TextureFormat::R32:
    case TextureFormat::RG32:
    case TextureFormat::RGB32:
    case TextureFormat::RGBA32:
    case TextureFormat::D24_S8:
    case TextureFormat::D32F:
        return 4;
    case TextureFormat::D32F_S8:
        return 8;
    case TextureFormat::R11G11B10F:
    case TextureFormat::R10G10B10A2:
        return 1; // packed; treat as 1 component (1x4)
    case TextureFormat::R16F:
    case TextureFormat::RG16F:
    case TextureFormat::RGB16F:
    case TextureFormat::RGBA16F:
        return 2;
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
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
        // undefined result
        return InvalidTextureFormat;
    }

    newNumComponents = MathUtil::Clamp(newNumComponents, uint8(1), uint8(4));

    int currentNumComponents = int(NumComponents(fmt));

    return TextureFormat(int(fmt) + int(newNumComponents) - currentNumComponents);
}

static inline constexpr bool IsDepthFormat(TextureFormat fmt)
{
    return fmt >= TextureFormat::D16;
}

static inline constexpr bool HasStencilComponent(TextureFormat fmt)
{
    return fmt == TextureFormat::D24_S8 || fmt == TextureFormat::D32F_S8;
}

static inline constexpr bool IsSRGB(TextureFormat fmt)
{
    return fmt >= TextureFormat::R8_SRGB && fmt < TextureFormat::D16;
}

/*! \brief Converts srgb formats to non-srgb variants and vice versa. Only for SRGB supported formats. */
static inline constexpr TextureFormat ChangeFormatSRGB(TextureFormat fmt, bool useSRGB = true)
{
    if (IsSRGB(fmt) == useSRGB)
    {
        return fmt;
    }

    constexpr uint32 BeginSRGB = uint32(TextureFormat::R8_SRGB);

    if (useSRGB)
    {
        TextureFormat srgbVersion = static_cast<TextureFormat>(uint32(fmt) + BeginSRGB);

        if (IsSRGB(srgbVersion))
        {
            return srgbVersion;
        }
    }
    else
    {
        int iFmt = int(fmt);
        if (iFmt - int(BeginSRGB) >= 0)
        {
            return static_cast<TextureFormat>(iFmt - int(BeginSRGB));
        }
    }

    return fmt;
}

static inline constexpr bool FormatSupportsBlending(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8:
    case TextureFormat::R8_SRGB:
    case TextureFormat::RG8:
    case TextureFormat::RG8_SRGB:
    case TextureFormat::RGB8:
    case TextureFormat::RGB8_SRGB:
    case TextureFormat::BGR8_SRGB:
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::R11G11B10F:
    case TextureFormat::R10G10B10A2:
    case TextureFormat::BGRA8:
    case TextureFormat::BGRA8_SRGB:
    case TextureFormat::R16F:
    case TextureFormat::RG16F:
    case TextureFormat::RGB16F:
    case TextureFormat::RGBA16F:
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return true;
    case TextureFormat::R16:
    case TextureFormat::RG16:
    case TextureFormat::RGB16:
    case TextureFormat::RGBA16:
    case TextureFormat::R32:
    case TextureFormat::RG32:
    case TextureFormat::RGB32:
    case TextureFormat::RGBA32:
    case TextureFormat::D16:
    case TextureFormat::D24_S8:
    case TextureFormat::D32F:
    case TextureFormat::D32F_S8:
    default:
        return false;
    }
}

} // namespace TextureUtils

template <size_t Size>
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

template <size_t Size>
using SizedUIntT = typename SizedUInt<Size>::Type;

template <TextureFormat Format>
struct TextureFormatHelper
{
    static constexpr uint32 NumComponents = TextureUtils::NumComponents(Format);
    static constexpr uint32 BytesPerComponent = TextureUtils::BytesPerComponent(Format);
    static constexpr bool IsSRGB = TextureUtils::IsSRGB(Format);
    static constexpr bool IsFloatingPoint = (Format >= TextureFormat::R16F && Format <= TextureFormat::RGBA32F);

    using ElementType = std::conditional_t<
        IsFloatingPoint,
        std::conditional_t<(Format <= TextureFormat::RGBA16F), Float16, float>,
        SizedUIntT<BytesPerComponent>>;
};

HYP_STRUCT()
struct TextureDesc
{
    HYP_STRUCT_BODY(TextureDesc);

    static constexpr uint8 MaxMips = 16;

    HYP_FIELD(Property = "Type", Serialize)
    TextureType type = TextureType::Texture2D;

    HYP_FIELD(Property = "Format", Serialize)
    TextureFormat format = TextureFormat::RGBA8;

    HYP_FIELD(Property = "Extent", Serialize)
    Vec3u extent = Vec3u::One();

    HYP_FIELD(Property = "MinFilterMode", Serialize)
    TextureFilterMode filterModeMin = TFM_NEAREST;

    HYP_FIELD(Property = "MagFilterMode", Serialize)
    TextureFilterMode filterModeMag = TFM_NEAREST;

    HYP_FIELD(Property = "TextureWrapMode", Serialize)
    TextureWrapMode wrapMode = TWM_CLAMP_TO_EDGE;

    HYP_FIELD(Property = "NumLayers", Serialize)
    uint16 numLayers = 1;

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

    uint8 NumMips() const
    {
        return HasMipMaps()
            ? MathUtil::Min(MaxMips, uint8(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y, extent.z))) + 1)
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
        return TextureUtils::IsSRGB(format);
    }

    HYP_FORCE_INLINE bool IsBlended() const
    {
        return imageUsage[IU_BLENDED];
    }

    HYP_FORCE_INLINE bool IsTextureCube() const
    {
        return type == TextureType::Cubemap;
    }

    HYP_FORCE_INLINE bool IsPanorama() const
    {
        return type == TextureType::Texture2D
            && extent.x == extent.y * 2
            && extent.z == 1;
    }

    HYP_FORCE_INLINE bool IsTexture2DArray() const
    {
        return type == TextureType::Texture2DArray;
    }

    HYP_FORCE_INLINE bool IsTextureCubeArray() const
    {
        return type == TextureType::CubemapArray;
    }

    HYP_FORCE_INLINE bool IsTexture3D() const
    {
        return type == TextureType::Texture3D;
    }

    HYP_FORCE_INLINE bool IsTexture2D() const
    {
        return type == TextureType::Texture2D;
    }

    uint16 NumArrayLayers() const
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

    size_t GetByteSize(bool includeAllMips = false) const
    {
        if (includeAllMips)
        {
            size_t totalByteSize = 0;

            const uint32 numMips = NumMips();

            for (uint32 mipIndex = 0; mipIndex < numMips; mipIndex++)
            {
                totalByteSize += GetMipByteSize(uint8(mipIndex), /* includeArrayLayers */ true);
            }

            return totalByteSize;
        }

        return (extent.x * extent.y * extent.z)
            * TextureUtils::BytesPerComponent(format)
            * TextureUtils::NumComponents(format)
            * NumArrayLayers();
    }

    size_t GetMipByteSize(uint8 mipIndex, bool includeArrayLayers = false) const
    {
        const uint32 numMips = NumMips();
        if (mipIndex >= numMips)
        {
            return 0;
        }

        const Vec3u mipExtent = GetMipExtent(mipIndex);

        return (mipExtent.x * mipExtent.y * mipExtent.z)
            * TextureUtils::BytesPerComponent(format)
            * TextureUtils::NumComponents(format)
            * (includeArrayLayers ? NumArrayLayers() : 1);
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

#pragma pack(push, 1)
struct PackedVertex
{
    float position[3];
    float normal[3];
    float uv[2];
};
#pragma pack(pop)

static_assert(sizeof(PackedVertex) == sizeof(float32) * 8);

HYP_ENUM()
enum class GpuBufferType : uint8
{
    NONE = 0,
    IndexBuffer,
    VertexBuffer,
    ConstantBuffer,
    StructuredBuffer,
    RWStructuredBuffer,
    ByteAddressBuffer,
    RWByteAddressBuffer,
    ReadbackBuffer,
    StagingBuffer,
    IndirectArgsBuffer,
    ShaderBindingTable,
    AccelerationStructureBuffer,
    AccelerationStructureInstanceBuffer,
    RTMeshIndexBuffer,
    RTMeshVertexBuffer,
    ScratchBuffer,
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

    constexpr BlendFunction()
        : BlendFunction(BMF_ONE, BMF_ZERO)
    {
    }

    constexpr BlendFunction(BlendModeFactor src, BlendModeFactor dst)
        : value((uint32(src) << 0) | (uint32(dst) << 4) | (uint32(src) << 8) | (uint32(dst) << 12))
    {
    }

    constexpr BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor, BlendModeFactor srcAlpha, BlendModeFactor dstAlpha)
        : value((uint32(srcColor) << 0) | (uint32(dstColor) << 4) | (uint32(srcAlpha) << 8) | (uint32(dstAlpha) << 12))
    {
    }

    constexpr BlendFunction(const BlendFunction& other) = default;
    BlendFunction& operator=(const BlendFunction& other) = default;

    constexpr BlendFunction(BlendFunction&& other) noexcept = default;
    BlendFunction& operator=(BlendFunction&& other) noexcept = default;

    ~BlendFunction() = default;

    HYP_FORCE_INLINE constexpr BlendModeFactor GetSrcColor() const
    {
        return BlendModeFactor(value & 0xF);
    }

    HYP_FORCE_INLINE void SetSrcColor(BlendModeFactor src)
    {
        value |= uint32(src);
    }

    HYP_FORCE_INLINE constexpr BlendModeFactor GetDstColor() const
    {
        return BlendModeFactor((value >> 4) & 0xF);
    }

    HYP_FORCE_INLINE void SetDstColor(BlendModeFactor dst)
    {
        value |= uint32(dst) << 4;
    }

    HYP_FORCE_INLINE constexpr BlendModeFactor GetSrcAlpha() const
    {
        return BlendModeFactor((value >> 8) & 0xF);
    }

    HYP_FORCE_INLINE void SetSrcAlpha(BlendModeFactor src)
    {
        value |= uint32(src) << 8;
    }

    HYP_FORCE_INLINE constexpr BlendModeFactor GetDstAlpha() const
    {
        return BlendModeFactor((value >> 12) & 0xF);
    }

    HYP_FORCE_INLINE void SetDstAlpha(BlendModeFactor dst)
    {
        value |= uint32(dst) << 12;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const BlendFunction& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const BlendFunction& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const BlendFunction& other) const
    {
        return value < other.value;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }

    HYP_FORCE_INLINE static constexpr BlendFunction None()
    {
        return BlendFunction(BMF_NONE, BMF_NONE);
    }

    HYP_FORCE_INLINE static constexpr BlendFunction Default()
    {
        return BlendFunction(BMF_ONE, BMF_ZERO);
    }

    HYP_FORCE_INLINE static constexpr BlendFunction AlphaBlending()
    {
        return BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ZERO);
    }

    HYP_FORCE_INLINE static constexpr BlendFunction Additive()
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
enum DepthCompareOp : uint8
{
    DCO_LESS,
    DCO_LESS_OR_EQUAL,
    DCO_GREATER,
    DCO_GREATER_OR_EQUAL,
    DCO_EQUAL,
    DCO_NOT_EQUAL,
    DCO_ALWAYS,
    DCO_NEVER
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

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Mat4f.hpp>

namespace Hyperion {

struct MeshDescription
{
    uint32 bindlessIndex;
    uint32 materialIndex;
    uint32 numIndices;
    uint32 numVertices;
};

struct ImageSubResource;

constexpr uint64 GetImageSubResourceKey(const ImageSubResource& subResource);

/* images */
struct ImageSubResource
{
    uint8 baseMipLevel = 0;
    uint8 numLevels = UINT8_MAX;

    uint16 baseArrayLayer = 0;
    uint16 numLayers = UINT16_MAX;

    bool operator==(const ImageSubResource& other) const
    {
        return baseMipLevel == other.baseMipLevel
            && numLevels == other.numLevels
            && baseArrayLayer == other.baseArrayLayer
            && numLayers == other.numLayers;
    }

    constexpr uint64 GetSubResourceKey() const
    {
        return GetImageSubResourceKey(*this);
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(GetImageSubResourceKey(*this));
    }
};

constexpr inline uint64 GetImageSubResourceKey(const ImageSubResource& subResource)
{
    return uint64(subResource.baseMipLevel) | (uint64(subResource.numLevels) << 8)
        | (uint64(subResource.baseArrayLayer) << 16) | (uint64(subResource.numLayers) << 32);
}

HYP_STRUCT()
struct Viewport
{
    HYP_STRUCT_BODY(Viewport);

    HYP_FIELD()
    Vec2u extent = Vec2u::One();

    HYP_FIELD()
    Vec2i position = Vec2i::Zero();

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
enum class ShaderResourceCategory : uint8
{
    Unknown,
    Buffer,
    Image,
    Sampler,
    AccelerationStructure
};

HYP_ENUM()
enum class ShaderInputType : uint8
{
    Unset,
    CBV,
    CBV_Dynamic,
    SRV,
    SRV_Dynamic,
    UAV,
    UAV_Dynamic,
    Sampler,
    MAX
};

HYP_ENUM()
enum class SamplerCompareOp : uint8
{
    None,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    Equal,
    NotEqual,
    Always,
    Never
};

#pragma pack(push, 1)

HYP_STRUCT()
struct SamplerDesc
{
    HYP_STRUCT_BODY(SamplerDesc);

    HYP_FIELD()
    TextureFilterMode minFilterMode;

    HYP_FIELD()
    TextureFilterMode magFilterMode;

    HYP_FIELD()
    TextureWrapMode wrapMode;

    HYP_FIELD()
    SamplerCompareOp compareOp;

    HYP_FORCE_INLINE bool operator==(const SamplerDesc& other) const
    {
        static_assert(sizeof(SamplerDesc) == sizeof(uint32) && std::has_unique_object_representations_v<SamplerDesc>);

        return *((const uint32*)this) == *((const uint32*)&other);
    }

    HYP_FORCE_INLINE bool operator!=(const SamplerDesc& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        static_assert(sizeof(SamplerDesc) == sizeof(uint32) && std::has_unique_object_representations_v<SamplerDesc>);

        return HashCode::GetHashCode(*(const uint32*)this);
    }
};

static constexpr uint32 ByteAddressBufferStride = 0;

#pragma pack(pop)

struct ShaderDataOffset
{
    size_t offset;
    size_t stride;

    constexpr ShaderDataOffset()
        : offset(size_t(-1)),
          stride(size_t(-1))
    {
    }

    constexpr ShaderDataOffset(size_t offset, size_t stride)
        : offset(offset),
          stride(stride)
    {
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return offset != size_t(-1)
            && stride != size_t(-1);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const ShaderDataOffset& other) const
    {
        return offset == other.offset
            && stride == other.stride;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderDataOffset& other) const
    {
        return offset != other.offset
            || stride != other.stride;
    }

    static constexpr ShaderDataOffset Invalid()
    {
        return ShaderDataOffset();
    }
};

template <class T>
struct TShaderDataOffset : ShaderDataOffset
{
    static_assert(is_pod_type_v<T>, "T must be POD to use with ShaderDataOffset");

    constexpr explicit TShaderDataOffset(uint32 index)
        : ShaderDataOffset()
    {
        offset = sizeof(T) * index;
        stride = sizeof(T);
    }

    constexpr explicit TShaderDataOffset(int index)
        : TShaderDataOffset(static_cast<uint32>(index))
    {
    }

    explicit TShaderDataOffset(const ObjectBase* resource)
        : ShaderDataOffset(size_t(-1), sizeof(T))
    {
        uint32 idx = Resources::GetBinding(resource);
        AssertDebug(idx != ~0u, "Invalid resource binding returned");

        if (idx != ~0u)
        {
            offset = sizeof(T) * idx;
        }
    }

    HYP_FORCE_INLINE static constexpr TShaderDataOffset Invalid()
    {
        return TShaderDataOffset();
    }
};

HYP_STRUCT()
struct DescriptorSetOffsetMap
{
    HYP_STRUCT_BODY(DescriptorSetOffsetMap);

    static constexpr uint32 MaxOffsets = 16; // 8;

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

    template <size_t Count>
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

enum class RenderPassMode : uint8
{
    RenderTarget,
    Present,

    Max
};

#pragma pack(push, 1)

struct AttachmentDesc
{
    TextureType imageType : 3;

    TextureFormat format : 6;

    LoadOperation loadOp : 2;
    StoreOperation storeOp : 2;

    bool onlyDepth : 1;
    bool onlyStencil : 1;
    bool clearColorIsF16 : 1;

    BlendFunction blendFunction;

    union
    {
        Color clearColor;
        Float16 clearColorF16[2];
    };

    AttachmentDesc()
    {
        static_assert(BlendFunction::None().value == 0);
        static_assert(uint8(TextureType::Texture2D) == 0);

        Memory::Zero(this, sizeof(AttachmentDesc));

        loadOp = LoadOperation::CLEAR;
        storeOp = StoreOperation::STORE;
        format = TextureFormat::RGBA8;
    }

    AttachmentDesc(
        TextureType textureType,
        TextureFormat format,
        LoadOperation loadOp = LoadOperation::CLEAR,
        StoreOperation storeOp = StoreOperation::STORE)
        : AttachmentDesc()
    {
        this->imageType = textureType;
        this->format = format;
        this->loadOp = loadOp;
        this->storeOp = storeOp;
    }

    AttachmentDesc(const AttachmentDesc& other) = default;
    AttachmentDesc& operator=(const AttachmentDesc& other) = default;

    HYP_FORCE_INLINE bool operator==(const AttachmentDesc& other) const
    {
        return std::memcmp(this, &other, sizeof(AttachmentDesc)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const AttachmentDesc& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode((const ubyte*)this, ((const ubyte*)this) + sizeof(AttachmentDesc));
    }
};

#pragma pack(pop)

struct FramebufferDesc
{
    static constexpr uint32 MaxAttachments = 5;

    Vec2u extent = Vec2u::One();
    Vec2i offset = Vec2i::Zero();

    uint32 numAttachments = 0;
    AttachmentDesc attachments[MaxAttachments];

    uint32 numLayers = 1;

    RenderPassMode renderPassMode = RenderPassMode::RenderTarget;

    HYP_FORCE_INLINE bool operator==(const FramebufferDesc& other) const
    {
        return extent == other.extent
            && offset == other.offset
            && numAttachments == other.numAttachments
            && std::equal(attachments, attachments + numAttachments, other.attachments)
            && numLayers == other.numLayers
            && renderPassMode == other.renderPassMode;
    }

    HYP_FORCE_INLINE bool operator!=(const FramebufferDesc& other) const
    {
        return !(*this == other);
    }

    void AddAttachment(const AttachmentDesc& attachmentDesc)
    {
        if (numAttachments >= MaxAttachments)
            return;

        attachments[numAttachments++] = attachmentDesc;
    }

    /*! \brief Test graphics pipeline compatibility: Can we reuse a graphics pipeline with \p other as its associated desc for this given desc? */
    bool IsPSOCompatible(const FramebufferDesc& other) const
    {
        return numAttachments == other.numAttachments
            && std::equal(attachments, attachments + numAttachments, other.attachments)
            && numLayers == other.numLayers
            && renderPassMode == other.renderPassMode;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(extent)
            .Combine(offset)
            .Combine(numAttachments)
            .Combine(HashCode::GetHashCode(attachments, attachments + numAttachments))
            .Combine(numLayers)
            .Combine(renderPassMode);
    }
};

/*! \brief Represents a set of vertex attributes used in mesh input.
 *  \details This struct is a bitmask representation of vertex attributes, allowing for efficient storage and manipulation of vertex attribute flags.
 *  It provides methods for checking, setting, and merging vertex attributes, as well as calculating the size of the vertex data based on the attributes. */
HYP_STRUCT(Serialize = "bitwise")
struct VertexTypeMask
{
    HYP_STRUCT_BODY(VertexTypeMask);

    HYP_FIELD()
    uint8 flagMask;

    constexpr VertexTypeMask()
        : flagMask(0)
    {
    }

    constexpr VertexTypeMask(uint8 flagMask)
        : flagMask(flagMask)
    {
    }

    constexpr VertexTypeMask(const VertexTypeMask& other) = default;
    VertexTypeMask& operator=(const VertexTypeMask& other) = default;

    ~VertexTypeMask() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return flagMask != 0;
    }

    HYP_FORCE_INLINE bool operator==(const VertexTypeMask& other) const
    {
        return flagMask == other.flagMask;
    }

    HYP_FORCE_INLINE bool operator!=(const VertexTypeMask& other) const
    {
        return flagMask != other.flagMask;
    }

    HYP_FORCE_INLINE VertexTypeMask operator~() const
    {
        return ~flagMask;
    }

    HYP_FORCE_INLINE VertexTypeMask operator&(const VertexTypeMask& other) const
    {
        return { uint8(flagMask & other.flagMask) };
    }

    HYP_FORCE_INLINE VertexTypeMask& operator&=(const VertexTypeMask& other)
    {
        flagMask &= other.flagMask;
        return *this;
    }

    HYP_FORCE_INLINE VertexTypeMask operator|(const VertexTypeMask& other) const
    {
        return { uint8(flagMask | other.flagMask) };
    }

    HYP_FORCE_INLINE VertexTypeMask& operator|=(const VertexTypeMask& other)
    {
        flagMask |= other.flagMask;
        return *this;
    }

    HYP_FORCE_INLINE VertexTypeMask operator|(VertexType attr) const
    {
        return { uint8(flagMask | attr) };
    }

    HYP_FORCE_INLINE VertexTypeMask& operator|=(VertexType attr)
    {
        flagMask |= attr;
        return *this;
    }

    HYP_FORCE_INLINE bool operator<(const VertexTypeMask& other) const
    {
        return flagMask < other.flagMask;
    }

    HYP_FORCE_INLINE bool Has(VertexType attr) const
    {
        return bool(*this & attr);
    }

    HYP_FORCE_INLINE void Set(VertexType attr, bool enable = true)
    {
        if (enable)
        {
            flagMask |= attr;
        }
        else
        {
            flagMask &= ~attr;
        }
    }

    HYP_FORCE_INLINE void Merge(const VertexTypeMask& other)
    {
        flagMask |= other.flagMask;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return uint32(ByteUtil::BitCount(flagMask));
    }

    ENGINE_API Array<VertexType> GetAllTypes() const;

    ENGINE_API String ToString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flagMask);

        return hc;
    }
};

HYP_STRUCT()
struct VertexAttributeDefinition
{
    HYP_STRUCT_BODY(VertexAttributeDefinition);

    HYP_FIELD()
    String name;

    HYP_FIELD()
    String typeClass;

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

enum class ShaderPropertyId : uint32;

HYP_STRUCT()
struct ShaderProperty
{
    HYP_STRUCT_BODY(ShaderProperty);

    using Value = Variant<Name, int, float>;

    HYP_FIELD(Property = "Name")
    Name name;

    HYP_FIELD(Property = "Flags")
    ShaderPropertyFlags flags;

    HYP_FIELD(Property = "Value")
    Value currentValue;

    HYP_FIELD(Property = "EnumValues")
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
        cachedHashCode = GetHashCode();
    }

    ShaderProperty(Name name, const Value& currentValue, ShaderPropertyFlags flags = SPF_NONE)
        : name(name),
          flags(flags),
          currentValue(currentValue)
    {
        cachedHashCode = GetHashCode();
    }

    explicit ShaderProperty(VertexType vt)
        : name(CreateNameFromDynamicString(ANSIString("VT_") + VertexUtils::ToString(vt))),
          flags(SPF_VERTEX_ATTRIBUTE),
          currentValue(Value(CreateNameFromDynamicString(VertexUtils::ToString(vt))))
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

    ENGINE_API String GetValueString() const;
    ENGINE_API HashCode GetHashCode() const;
    ENGINE_API String ToString() const;
};

HYP_STRUCT()
class ShaderVariantPerms
{
    friend class ShaderCompiler;

public:
    HYP_STRUCT_BODY(ShaderVariantPerms);

    using Iterator = typename TSet<ShaderProperty>::Iterator;
    using ConstIterator = typename TSet<ShaderProperty>::ConstIterator;

    ShaderVariantPerms()
        : m_needsHashCodeRecalculation(true)
    {
    }

    explicit ShaderVariantPerms(const TSet<ShaderProperty>& props)
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property, true);
        }
    }

    template <size_t Sz>
    ShaderVariantPerms(Name const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            Set(ShaderProperty(propKey, SPF_PERMUTATION), true); // default to permutable
        }
    }

    template <size_t Sz>
    ShaderVariantPerms(ShaderProperty const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property, true);
        }
    }

    template <size_t Sz>
    ShaderVariantPerms(const VertexTypeMask& vertexAttributes, Name const (&props)[Sz])
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            m_props.Insert(ShaderProperty(propKey, SPF_PERMUTATION)); // default to permutable
        }
    }

    explicit ShaderVariantPerms(const VertexTypeMask& vertexAttributes)
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
    }

    ShaderVariantPerms(const ShaderVariantPerms& other) = default;
    ShaderVariantPerms& operator=(const ShaderVariantPerms& other) = default;

    ShaderVariantPerms(ShaderVariantPerms&& other) noexcept = default;
    ShaderVariantPerms& operator=(ShaderVariantPerms&& other) = default;

    ~ShaderVariantPerms() = default;

    HYP_FORCE_INLINE bool operator==(const ShaderVariantPerms& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ShaderVariantPerms& other) const = delete;

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
        return const_cast<ShaderVariantPerms*>(this)->Find(property);
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
        return const_cast<ShaderVariantPerms*>(this)->Find(name);
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttributes(VertexTypeMask vertexAttributes) const
    {
        return (m_requiredVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttribute(VertexType vt) const
    {
        return m_requiredVertexAttributes.Has(vt);
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttributes(VertexType vertexAttributes) const
    {
        return (m_optionalVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttribute(VertexType vt) const
    {
        return m_optionalVertexAttributes.Has(vt);
    }

    HYP_FORCE_INLINE bool Has(const ShaderProperty& property) const
    {
        return Find(property) != m_props.End();
    }

    HYP_FORCE_INLINE bool Has(StringHash name) const
    {
        return Find(name) != m_props.End();
    }

    ENGINE_API ShaderVariantPerms& Set(const ShaderProperty& property, bool enabled = true);

    HYP_FORCE_INLINE ShaderVariantPerms& Set(Name name, bool enabled = true, ShaderPropertyFlags flags = SPF_NONE)
    {
        return Set(ShaderProperty(name, flags), enabled);
    }

    /*! \brief Applies \p other properties onto this set */
    void Merge(const ShaderVariantPerms& other)
    {
        for (const ShaderProperty& property : other.m_props)
        {
            Set(property, true);
        }

        m_requiredVertexAttributes |= other.m_requiredVertexAttributes;
        m_optionalVertexAttributes |= other.m_optionalVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    static ShaderVariantPerms Merge(const ShaderVariantPerms& a, const ShaderVariantPerms& b)
    {
        ShaderVariantPerms result(a);
        result.Merge(b);

        return result;
    }

    HYP_FORCE_INLINE const TSet<ShaderProperty>& GetPropertySet() const
    {
        return m_props;
    }

    /*! \brief Adds a new permutation shader property
     *  Permutations create new shader variants based on their values.
     *  Many permutations will drastically increase the number of shader variants generated,
     *  so use them sparingly. (prefer value groups or static properties where appropriate) */
    ShaderVariantPerms& AddPermutation(Name key)
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
    ShaderVariantPerms& AddStatic(Name key)
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

    /*! \brief Adds a new static property with key \p key and a fixed \p value.
     *  Static properties are applied to every shader variant and do not create new permutations. */
    ShaderVariantPerms& AddStatic(Name key, ShaderProperty::Value value)
    {
        const ShaderProperty shaderProperty(key, value, SPF_NONE);

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
    ShaderVariantPerms& AddValueGroup(Name key, const Array<ShaderProperty::Value>& enumValues)
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

    HYP_FORCE_INLINE VertexTypeMask GetRequiredVertexAttributes() const
    {
        return m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE VertexTypeMask GetOptionalVertexAttributes() const
    {
        return m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE VertexTypeMask GetAllVertexAttributes() const
    {
        return m_requiredVertexAttributes | m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE void SetRequiredVertexAttributes(VertexTypeMask vertexAttributes)
    {
        m_requiredVertexAttributes = vertexAttributes;
        m_optionalVertexAttributes = m_optionalVertexAttributes & ~m_requiredVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE void SetOptionalVertexAttributes(VertexTypeMask vertexAttributes)
    {
        m_optionalVertexAttributes = vertexAttributes & ~m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_props.Size();
    }

    HYP_FORCE_INLINE Array<ShaderProperty> ToArray() const
    {
        return m_props.ToArray();
    }

    ENGINE_API String ToString() const;

    HashCode GetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode_Const();
        }

        return m_cachedHashCode;
    }

    HashCode GetPropertySetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode_Const();
        }

        return m_cachedPropertySetHashCode;
    }

    HashCode RecalculateHashCode()
    {
        return RecalculateHashCode_Const();
    }

    HYP_DEF_STL_BEGIN_END(m_props.Begin(), m_props.End());

private:
    HashCode RecalculateHashCode_Const() const
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

        for (const ShaderProperty* property : propsPtrs)
        {
            m_cachedPropertySetHashCode.Add(property->GetHashCode());
        }

        hc.Add(m_cachedPropertySetHashCode);

        m_cachedHashCode = hc;
        m_needsHashCodeRecalculation = false;

        return hc;
    }

    HYP_FIELD()
    TSet<ShaderProperty> m_props;

    HYP_FIELD()
    VertexTypeMask m_requiredVertexAttributes;

    HYP_FIELD()
    VertexTypeMask m_optionalVertexAttributes;

    mutable HashCode m_cachedHashCode;
    mutable HashCode m_cachedPropertySetHashCode;
    mutable bool m_needsHashCodeRecalculation;
};

HYP_STRUCT()
struct ShaderPropertySet
{
    HYP_STRUCT_BODY(ShaderPropertySet);

    static constexpr uint32 NumChunks = 8;
    static constexpr uint32 ChunkSize = sizeof(uint32);
    static constexpr uint32 ChunkSizeBits = ChunkSize * CHAR_BIT;
    static constexpr uint32 MaxProperties = ChunkSize * NumChunks;

    HYP_FIELD()
    FixedArray<uint32, NumChunks> chunks;

    HYP_FORCE_INLINE constexpr ShaderPropertySet()
        : chunks{}
    {
    }

    template <size_t N>
    constexpr ShaderPropertySet(const ShaderPropertyId(&properties)[N])
    {
        for (auto it = std::begin(properties); it != std::end(properties); ++it)
        {
            Add(*it);
        }
    }

    constexpr ShaderPropertySet(const ShaderPropertySet& other) = default;
    constexpr ShaderPropertySet& operator=(const ShaderPropertySet& other) = default;

    HYP_FORCE_INLINE void Add(ShaderPropertyId id)
    {
        AssertDebug(uint32(id) < ChunkSizeBits * NumChunks);

        chunks[uint32(id) / ChunkSizeBits] |= (1ull << (uint32(id) % ChunkSizeBits));
    }

    HYP_FORCE_INLINE void Set(ShaderPropertyId id, bool enable)
    {
        AssertDebug(uint32(id) < ChunkSizeBits * NumChunks);

        if (enable)
        {
            chunks[uint32(id) / ChunkSizeBits] |= (1ull << (uint32(id) % ChunkSizeBits));
        }
        else
        {
            chunks[uint32(id) / ChunkSizeBits] &= ~(1ull << (uint32(id) % ChunkSizeBits));
        }
    }

    HYP_FORCE_INLINE constexpr bool Test(ShaderPropertyId id) const
    {
        return bool(chunks[0] & (1u << uint32(id)))
            || bool(chunks[1] & (1u << uint32(id)))
            || bool(chunks[2] & (1u << uint32(id)))
            || bool(chunks[3] & (1u << uint32(id)));
    }

    HYP_FORCE_INLINE bool operator==(const ShaderPropertySet& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderPropertySet)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderPropertySet& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderPropertySet)) != 0;
    }

    HYP_FORCE_INLINE constexpr ShaderPropertySet operator&(const ShaderPropertySet& other) const
    {
        ShaderPropertySet result = *this;
        result.chunks[0] &= other.chunks[0];
        result.chunks[1] &= other.chunks[1];
        result.chunks[2] &= other.chunks[2];
        result.chunks[3] &= other.chunks[3];

        return result;
    }

    HYP_FORCE_INLINE constexpr ShaderPropertySet operator|(const ShaderPropertySet& other) const
    {
        ShaderPropertySet result = *this;
        result.chunks[0] |= other.chunks[0];
        result.chunks[1] |= other.chunks[1];
        result.chunks[2] |= other.chunks[2];
        result.chunks[3] |= other.chunks[3];

        return result;
    }

    Array<ShaderPropertyId> ToArray() const;
    String GetDebugString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(this),
            reinterpret_cast<const ubyte*>(this) + sizeof(ShaderPropertySet));
    }
};

extern ShaderPropertyId InternShaderProperty(const ShaderProperty& shaderProperty);

/*! \brief For requested shader instance */
struct ShaderDesc
{
    static constexpr uint32 MaxShaderProperties = 8;

    Name name;
    ShaderPropertySet properties;

    ShaderDesc() = default;

    explicit ShaderDesc(Name name)
        : name(name),
          properties{}
    {
    }

    template <size_t N>
    constexpr ShaderDesc(Name name, const ShaderPropertyId(&propertyIds)[N])
        : name(name)
    {
        for (uint32 i = 0; i < N; i++)
        {
            properties.Add(propertyIds[i]);
        }
    }

    explicit constexpr ShaderDesc(Name name, const ShaderPropertySet& properties)
        : name(name),
          properties(properties)
    {
    }

    ShaderDesc(const ShaderDesc& other) = default;
    ShaderDesc& operator=(const ShaderDesc& other) = default;

    HYP_FORCE_INLINE bool operator==(const ShaderDesc& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderDesc)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderDesc& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderDesc)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(this),
            reinterpret_cast<const ubyte*>(this) + sizeof(ShaderDesc));
    }
};

#pragma pack(push, 1)

struct ShaderUniform
{
    Name name;

    union
    {
        GpuBuffer* buffer;
        GpuImageView* imageView;
        Sampler* sampler;
        GpuTlas* tlas;
    };

    enum : uint32
    {
        UT_Buffer,
        UT_ImageView,
        UT_Sampler,
        UT_Tlas
    } type;

    ShaderUniform() = default;

    ShaderUniform(const ShaderUniform& other)
        : name(other.name),
          buffer(other.buffer),
          type(other.type)
    {
    }

    ShaderUniform(StringHash name, GpuBuffer* buffer)
        : name(name),
          buffer(buffer),
          type(UT_Buffer)
    {
    }

    ShaderUniform(StringHash name, GpuImageView* imageView)
        : name(name),
          imageView(imageView),
          type(UT_ImageView)
    {
    }

    ShaderUniform(StringHash name, Sampler* sampler)
        : name(name),
          sampler(sampler),
          type(UT_Sampler)
    {
    }

    ShaderUniform(StringHash name, GpuTlas* tlas)
        : name(name),
          tlas(tlas),
          type(UT_Tlas)
    {
    }

    HYP_FORCE_INLINE bool operator==(const ShaderUniform& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderUniform)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderUniform& other) const
    {
        return std::memcmp(this, &other, sizeof(ShaderUniform)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(this),
            reinterpret_cast<const ubyte*>(this) + sizeof(ShaderUniform));
    }
};

struct ShaderUniforms
{
    static constexpr size_t MaxUniforms = 16;

    ShaderUniform uniforms[MaxUniforms] {};
    size_t bufferOffsets[MaxUniforms] {};
    size_t bufferStrides[MaxUniforms] {};
    uint8 count = 0;

    HYP_FORCE_INLINE void SetBuffer(StringHash name, GpuBuffer* buffer, size_t offset, size_t stride)
    {
        uint8 index = count++;
        Assert(index < MaxUniforms);

        uniforms[index] = ShaderUniform(name, buffer);
        bufferOffsets[index] = offset;
        bufferStrides[index] = stride;
    }

    HYP_FORCE_INLINE void SetImageView(StringHash name, GpuImageView* imageView)
    {
        uint8 index = count++;
        Assert(index < MaxUniforms);

        uniforms[index] = ShaderUniform(name, imageView);
    }

    HYP_FORCE_INLINE void SetSampler(StringHash name, Sampler* sampler)
    {
        uint8 index = count++;
        Assert(index < MaxUniforms);

        uniforms[index] = ShaderUniform(name, sampler);
    }

    HYP_FORCE_INLINE void SetTlas(StringHash name, GpuTlas* tlas)
    {
        uint8 index = count++;
        Assert(index < MaxUniforms);

        uniforms[index] = ShaderUniform(name, tlas);
    }
};

#pragma pack(pop)

HYP_ENUM()
enum class ShaderModuleType : uint8
{
    None = 0,

    /* Graphics and general purpose shaders */
    Vertex,
    Pixel,
    Geometry,
    Compute,

    /* Mesh shaders */
    Task,
    Mesh,

    /* Tesselation */
    TessControl,
    TessEval,

    /* RayTracing hardware specific */
    RayGen,
    Intersect,
    AnyHit,
    ClosestHit,
    Miss,

    Max
};

static constexpr uint8 NumShaderModuleTypes = uint8(ShaderModuleType::Max);

static constexpr inline bool IsRayTracingShaderModule(ShaderModuleType type)
{
    return type == ShaderModuleType::RayGen
        || type == ShaderModuleType::Intersect
        || type == ShaderModuleType::AnyHit
        || type == ShaderModuleType::ClosestHit
        || type == ShaderModuleType::Miss;
}

struct ShaderBundleDecl // combination of shader files, .frag, .vert etc. in .ini definitions file.
{
    Name name;
    FlatMap<ShaderModuleType, String> sources;
    ShaderVariantPerms variantPerms;

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
                return item.first == ShaderModuleType::Compute;
            });
    }

    bool HasVertexShader() const
    {
        return AnyOf(sources, [](const KeyValuePair<ShaderModuleType, String>& item)
            {
                return item.first == ShaderModuleType::Vertex;
            });
    }
};

} // namespace Hyperion
