/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/math/Extent.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

enum ImageUsage : uint32
{
    IU_NONE = 0x0,
    IU_SAMPLED = 0x1,
    IU_STORAGE = 0x2,
    IU_ATTACHMENT = 0x4,
    IU_BLENDED = 0x8
};

HYP_MAKE_ENUM_FLAGS(ImageUsage);

enum ImageSupport : uint8
{
    IS_SRV,
    IS_UAV,
    IS_DEPTH
};

enum DefaultImageFormat : uint8
{
    DIF_NONE,
    DIF_COLOR,
    DIF_DEPTH,
    DIF_NORMALS,
    DIF_STORAGE
};

enum TextureType : uint32
{
    TT_INVALID = uint32(-1),

    TT_TEX2D = 0,
    TT_TEX3D = 1,
    TT_CUBEMAP = 2,
    TT_TEX2D_ARRAY = 3,
    TT_CUBEMAP_ARRAY = 4,

    TT_MAX
};

enum TextureBaseFormat : uint32
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

enum TextureFormat : uint32
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

enum TextureFilterMode : uint32
{
    TFM_NEAREST,
    TFM_LINEAR,
    TFM_NEAREST_LINEAR,
    TFM_NEAREST_MIPMAP,
    TFM_LINEAR_MIPMAP,
    TFM_MINMAX_MIPMAP
};

enum TextureWrapMode : uint32
{
    TWM_CLAMP_TO_EDGE,
    TWM_CLAMP_TO_BORDER,
    TWM_REPEAT
};

enum ResourceState : uint32
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

enum BufferIDMask : uint64
{
    ID_MASK_BUFFER = (0x1ull << 32ull),
    ID_MASK_IMAGE = (0x2ull << 32ull)
};

HYP_ENUM()
enum ShaderModuleType : uint32
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

    /* Raytracing hardware specific */
    SMT_RAY_GEN,
    SMT_RAY_INTERSECT,
    SMT_RAY_ANY_HIT,
    SMT_RAY_CLOSEST_HIT,
    SMT_RAY_MISS,

    SMT_MAX
};

static inline bool IsRaytracingShaderModule(ShaderModuleType type)
{
    return type == SMT_RAY_GEN
        || type == SMT_RAY_INTERSECT
        || type == SMT_RAY_ANY_HIT
        || type == SMT_RAY_CLOSEST_HIT
        || type == SMT_RAY_MISS;
}

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
    case TF_R11G11B10F:
    case TF_RGB16:
    case TF_RGB32:
    case TF_RGB16F:
    case TF_RGB32F:
        return TFB_RGB;
    case TF_RGBA8:
    case TF_RGBA8_SRGB:
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
    case TF_R11G11B10F:
    case TF_DEPTH_24:
    case TF_DEPTH_32F:
        return 4;
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
    case TF_R10G10B10A2:
    case TF_R11G11B10F:
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

HYP_STRUCT()
struct TextureDesc
{
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

    HYP_FORCE_INLINE bool operator==(const TextureDesc& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const TextureDesc& other) const = default;

    HYP_FORCE_INLINE bool HasMipmaps() const
    {
        return filterModeMin == TFM_NEAREST_MIPMAP
            || filterModeMin == TFM_LINEAR_MIPMAP
            || filterModeMin == TFM_MINMAX_MIPMAP;
    }

    HYP_FORCE_INLINE uint32 NumMipmaps() const
    {
        return HasMipmaps()
            ? uint32(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y, extent.z))) + 1
            : 1;
    }

    HYP_FORCE_INLINE bool IsDepthStencil() const
    {
        return IsDepthFormat(format);
    }

    HYP_FORCE_INLINE bool IsSrgb() const
    {
        return IsSrgbFormat(format);
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

    HYP_FORCE_INLINE uint32 NumFaces() const
    {
        const uint32 numArrayLayers = numLayers;

        if (IsTextureCube() || IsTextureCubeArray())
        {
            return 6 * numArrayLayers;
        }

        return numArrayLayers;
    }

    HYP_FORCE_INLINE uint32 GetByteSize() const
    {
        return uint32(extent.x * extent.y * extent.z)
            * BytesPerComponent(format)
            * NumComponents(format)
            * NumFaces();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(format);
        hc.Add(extent);
        hc.Add(filterModeMin);
        hc.Add(filterModeMag);
        hc.Add(wrapMode);
        hc.Add(numLayers);

        return hc;
    }
};

HYP_STRUCT()
struct TextureData
{
    HYP_FIELD(Property = "TextureDesc", Serialize)
    TextureDesc desc;

    HYP_FIELD(Property = "ImageData", Serialize, Compressed)
    ByteBuffer imageData;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return imageData.Any();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(desc.GetHashCode());
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
enum GpuElemType : uint32
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
enum FaceCullMode : uint32
{
    FCM_NONE,
    FCM_BACK,
    FCM_FRONT
};

HYP_ENUM()
enum FillMode : uint32
{
    FM_FILL,
    FM_LINE
};

HYP_ENUM()
enum Topology : uint32
{
    TOP_TRIANGLES,
    TOP_TRIANGLE_FAN,
    TOP_TRIANGLE_STRIP,

    TOP_LINES,

    TOP_POINTS
};

HYP_ENUM()
enum BlendModeFactor : uint32
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

struct BlendFunction
{
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

enum StencilCompareOp : uint8
{
    SCO_ALWAYS,
    SCO_NEVER,
    SCO_EQUAL,
    SCO_NOT_EQUAL
};

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
    HYP_FIELD()
    StencilOp passOp = SO_KEEP;

    HYP_FIELD()
    StencilOp failOp = SO_REPLACE;

    HYP_FIELD()
    StencilOp depthFailOp = SO_REPLACE;

    HYP_FIELD()
    StencilCompareOp compareOp = SCO_ALWAYS;

    HYP_FIELD()
    uint8 mask = 0x0;

    HYP_FIELD()
    uint8 value = 0x1;

    HYP_FORCE_INLINE bool operator==(const StencilFunction& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const StencilFunction& other) const = default;

    HYP_FORCE_INLINE bool operator<(const StencilFunction& other) const
    {
        return Memory::MemCmp(this, &other, sizeof(StencilFunction)) < 0;
    }

    HYP_FORCE_INLINE bool IsSet() const
    {
        return mask != 0x0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
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

        Memory::MemCpy(&data[0], ptr, size);
    }

    template <class T>
    PushConstantData(const T* value)
        : size(uint32(sizeof(T)))
    {
        static_assert(sizeof(T) <= 128, "Push constant data size exceeds 128 bytes");
        static_assert(std::is_trivial_v<T>, "T must be a trivial type");
        static_assert(std::is_standard_layout_v<T>, "T must be a standard layout type");

        Memory::MemCpy(&data[0], value, sizeof(T));
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

enum DescriptorSlot : uint32
{
    DESCRIPTOR_SLOT_NONE,
    DESCRIPTOR_SLOT_SRV,
    DESCRIPTOR_SLOT_UAV,
    DESCRIPTOR_SLOT_CBUFF,
    DESCRIPTOR_SLOT_SSBO,
    DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE,
    DESCRIPTOR_SLOT_SAMPLER,
    DESCRIPTOR_SLOT_MAX
};

struct alignas(16) MeshDescription
{
    uint64 vertexBufferAddress;
    uint64 indexBufferAddress;

    uint32 _pad0;
    uint32 materialIndex;
    uint32 numIndices;
    uint32 numVertices;
};

using ImageSubResourceFlagBits = uint32;

enum ImageSubResourceFlags : ImageSubResourceFlagBits
{
    IMAGE_SUB_RESOURCE_FLAGS_NONE = 0,
    IMAGE_SUB_RESOURCE_FLAGS_COLOR = 1 << 0,
    IMAGE_SUB_RESOURCE_FLAGS_DEPTH = 1 << 1,
    IMAGE_SUB_RESOURCE_FLAGS_STENCIL = 1 << 2
};

static inline uint64 GetImageSubResourceKey(uint32 baseArrayLayer, uint32 baseMipLevel)
{
    return (uint64(baseArrayLayer) << 32) | (uint64(baseMipLevel));
}

/* images */
struct ImageSubResource
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    uint32 baseArrayLayer = 0;
    uint32 baseMipLevel = 0;
    uint32 numLayers = 1;
    uint32 numLevels = 1;

    bool operator==(const ImageSubResource& other) const
    {
        return flags == other.flags
            && baseArrayLayer == other.baseArrayLayer
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
        hc.Add(flags);
        hc.Add(baseArrayLayer);
        hc.Add(numLayers);
        hc.Add(baseMipLevel);
        hc.Add(numLevels);

        return hc;
    }
};

struct Viewport
{
    Vec2u extent;
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

} // namespace hyperion
