/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_STRUCTS_HPP
#define HYPERION_BACKEND_RENDERER_STRUCTS_HPP

#include <core/containers/String.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/Defines.hpp>
#include <util/EnumOptions.hpp>
#include <math/Extent.hpp>
#include <math/Vector2.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

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

HYP_STRUCT()
struct TextureDesc
{
    HYP_FIELD(Serialize, Property="Type")
    ImageType       type = ImageType::TEXTURE_TYPE_2D;

    HYP_FIELD(Serialize, Property="Format")
    InternalFormat  format = InternalFormat::RGBA8;

    HYP_FIELD(Serialize, Property="Extent")
    Vec3u           extent = Vec3u::One();

    HYP_FIELD(Serialize, Property="MinFilterMode")
    FilterMode      filter_mode_min = FilterMode::TEXTURE_FILTER_NEAREST;

    HYP_FIELD(Serialize, Property="MagFilterMode")
    FilterMode      filter_mode_mag = FilterMode::TEXTURE_FILTER_NEAREST;

    HYP_FIELD(Serialize, Property="WrapMode")
    WrapMode        wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE;

    HYP_FIELD(Serialize, Property="NumLayers")
    uint32          num_layers = 1;

    HYP_FIELD(Serialize, Property="NumFaces")
    uint32          num_faces = 1;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(format);
        hc.Add(extent);
        hc.Add(filter_mode_min);
        hc.Add(filter_mode_mag);
        hc.Add(wrap_mode);
        hc.Add(num_layers);
        hc.Add(num_faces);

        return hc;
    }
};

struct TextureData
{
    TextureDesc desc;
    ByteBuffer  buffer;

    HYP_FORCE_INLINE bool IsValid() const
        { return buffer.Any(); }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(desc.GetHashCode());
        hc.Add(buffer.GetHashCode());

        return hc;
    }
};

struct alignas(16) PackedVertex
{
    float   position_x,
            position_y,
            position_z,
            normal_x,
            normal_y,
            normal_z,
            texcoord0_x,
            texcoord0_y;
};

static_assert(sizeof(PackedVertex) == sizeof(float32) * 8);

enum class DatumType : uint32
{
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class FaceCullMode : uint32
{
    NONE,
    BACK,
    FRONT
};

enum class FillMode : uint32
{
    FILL,
    LINE
};

enum class Topology : uint32
{
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

enum class StencilMode : uint8
{
    NONE,
    FILL,
    OUTLINE
};

enum class BlendModeFactor : uint32
{
    NONE,

    ONE,
    ZERO,
    SRC_COLOR,
    SRC_ALPHA,
    DST_COLOR,
    DST_ALPHA,
    ONE_MINUS_SRC_COLOR,
    ONE_MINUS_SRC_ALPHA,
    ONE_MINUS_DST_COLOR,
    ONE_MINUS_DST_ALPHA,

    MAX
};

static_assert(uint32(BlendModeFactor::MAX) <= 15, "BlendModeFactor enum too large to fit in 4 bits");

struct BlendFunction
{
    uint32  value;

    BlendFunction()
        : BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ZERO)
    {
    }

    BlendFunction(BlendModeFactor src, BlendModeFactor dst)
        : value((uint32(src) << 0) | (uint32(dst) << 4) | (uint32(src) << 8) | (uint32(dst) << 12))
    {
    }

    BlendFunction(BlendModeFactor src_color, BlendModeFactor dst_color, BlendModeFactor src_alpha, BlendModeFactor dst_alpha)
        : value((uint32(src_color) << 0) | (uint32(dst_color) << 4) | (uint32(src_alpha) << 8) | (uint32(dst_alpha) << 12))
    {
    }

    BlendFunction(const BlendFunction &other)                   = default;
    BlendFunction &operator=(const BlendFunction &other)        = default;
    BlendFunction(BlendFunction &&other) noexcept               = default;
    BlendFunction &operator=(BlendFunction &&other) noexcept    = default;
    ~BlendFunction()                                            = default;

    HYP_FORCE_INLINE BlendModeFactor GetSrcColor() const
        { return BlendModeFactor(value & 0xF); }

    HYP_FORCE_INLINE void SetSrcColor(BlendModeFactor src)
        { value |= uint32(src); }

    HYP_FORCE_INLINE BlendModeFactor GetDstColor() const
        { return BlendModeFactor((value >> 4) & 0xF); }

    HYP_FORCE_INLINE void SetDstColor(BlendModeFactor dst)
        { value |= uint32(dst) << 4; }

    HYP_FORCE_INLINE BlendModeFactor GetSrcAlpha() const
        { return BlendModeFactor((value >> 8) & 0xF); }

    HYP_FORCE_INLINE void SetSrcAlpha(BlendModeFactor src)
        { value |= uint32(src) << 8; }

    HYP_FORCE_INLINE BlendModeFactor GetDstAlpha() const
        { return BlendModeFactor((value >> 12) & 0xF); }

    HYP_FORCE_INLINE void SetDstAlpha(BlendModeFactor dst)
        { value |= uint32(dst) << 12; }

    HYP_FORCE_INLINE bool operator==(const BlendFunction &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const BlendFunction &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool operator<(const BlendFunction &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }

    HYP_FORCE_INLINE static BlendFunction None()
        { return BlendFunction(BlendModeFactor::NONE, BlendModeFactor::NONE); }

    HYP_FORCE_INLINE static BlendFunction Default()
        { return BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ZERO); }

    HYP_FORCE_INLINE static BlendFunction AlphaBlending()
        { return BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA, BlendModeFactor::ONE, BlendModeFactor::ZERO); }

    HYP_FORCE_INLINE static BlendFunction Additive()
        { return BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ONE); }
};

enum class StencilCompareOp : uint8
{
    ALWAYS,
    NEVER,
    EQUAL,
    NOT_EQUAL
};

enum class StencilOp : uint8
{
    KEEP,
    ZERO,
    REPLACE,
    INCREMENT,
    DECREMENT
};

HYP_STRUCT()
struct StencilFunction
{
    HYP_FIELD()
    StencilOp           pass_op = StencilOp::KEEP;

    HYP_FIELD()
    StencilOp           fail_op = StencilOp::REPLACE;

    HYP_FIELD()
    StencilOp           depth_fail_op = StencilOp::REPLACE;

    HYP_FIELD()
    StencilCompareOp    compare_op = StencilCompareOp::ALWAYS;

    HYP_FIELD()
    uint8               mask = 0x0;

    HYP_FIELD()
    uint8               value = 0x1;

    HYP_FORCE_INLINE bool operator==(const StencilFunction &other) const = default;
    HYP_FORCE_INLINE bool operator!=(const StencilFunction &other) const = default;

    HYP_FORCE_INLINE bool operator<(const StencilFunction &other) const
        { return Memory::MemCmp(this, &other, sizeof(StencilFunction)) < 0; }

    HYP_FORCE_INLINE bool IsSet() const
        { return mask != 0x0; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
};

struct PushConstantData
{
    ubyte   data[128];
    uint32  size;

    PushConstantData()
        : data { 0 },
          size(0)
    {
    }

    PushConstantData(const void *ptr, uint32 size)
        : size(size)
    {
        AssertThrowMsg(size <= 128, "Push constant data size exceeds 128 bytes");

        Memory::MemCpy(&data[0], ptr, size);
    }

    template <class T>
    PushConstantData(const T *value)
        : size(uint32(sizeof(T)))
    {
        static_assert(sizeof(T) <= 128, "Push constant data size exceeds 128 bytes");
        static_assert(std::is_trivial_v<T>, "T must be a trivial type");
        static_assert(std::is_standard_layout_v<T>, "T must be a standard layout type");

        Memory::MemCpy(&data[0], value, sizeof(T));
    }

    PushConstantData(const PushConstantData &other)                 = default;
    PushConstantData &operator=(const PushConstantData &other)      = default;
    PushConstantData(PushConstantData &&other) noexcept             = default;
    PushConstantData &operator=(PushConstantData &&other) noexcept  = default;
    ~PushConstantData()                                             = default;

    HYP_FORCE_INLINE const void *Data() const
        { return data; }

    HYP_FORCE_INLINE uint32 Size() const
        { return size; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return size != 0; }
};

} // namespace hyperion::renderer

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererStructs.hpp>
#else
#error Unsupported rendering backend
#endif

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

struct alignas(16) MeshDescription
{
    uint64 vertex_buffer_address;
    uint64 index_buffer_address;

    uint32 entity_index;
    uint32 material_index;
    uint32 num_indices;
    uint32 num_vertices;
};

using ImageSubResourceFlagBits = uint32;

enum ImageSubResourceFlags : ImageSubResourceFlagBits
{
    IMAGE_SUB_RESOURCE_FLAGS_NONE    = 0,
    IMAGE_SUB_RESOURCE_FLAGS_COLOR   = 1 << 0,
    IMAGE_SUB_RESOURCE_FLAGS_DEPTH   = 1 << 1,
    IMAGE_SUB_RESOURCE_FLAGS_STENCIL = 1 << 2
};

/* images */
struct ImageSubResource
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    uint32 base_array_layer = 0;
    uint32 base_mip_level = 0;
    uint32 num_layers = 1;
    uint32 num_levels = 1;

    bool operator==(const ImageSubResource &other) const
    {
        return flags == other.flags
            && base_array_layer == other.base_array_layer
            && num_layers == other.num_layers
            && base_mip_level == other.base_mip_level
            && num_levels == other.num_levels;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flags);
        hc.Add(base_array_layer);
        hc.Add(num_layers);
        hc.Add(base_mip_level);
        hc.Add(num_levels);

        return hc;
    }
};

struct Viewport
{
    Vec2i   position;
    Vec2i   extent;

    HYP_FORCE_INLINE explicit operator bool() const
        { return position != Vec2i::Zero() || extent != Vec2i::Zero(); }

    HYP_FORCE_INLINE bool operator!() const
        { return position == Vec2i::Zero() && extent == Vec2i::Zero(); }

    HYP_FORCE_INLINE bool operator==(const Viewport &other) const
        { return position == other.position && extent == other.extent; }

    HYP_FORCE_INLINE bool operator!=(const Viewport &other) const
        { return position != other.position || extent != other.extent; }
};

} // namespace renderer

using renderer::ImageType;
using renderer::BaseFormat;
using renderer::InternalFormat;
using renderer::FilterMode;
using renderer::WrapMode;
using renderer::TextureMode;
using renderer::TextureData;
using renderer::TextureDesc;

} // namespace hyperion

#endif
