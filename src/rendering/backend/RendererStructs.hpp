#ifndef HYPERION_V2_BACKEND_RENDERER_STRUCTS_H
#define HYPERION_V2_BACKEND_RENDERER_STRUCTS_H

#include <core/lib/String.hpp>
#include <util/Defines.hpp>
#include <util/EnumOptions.hpp>
#include <math/Extent.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <tuple>

namespace hyperion::renderer {

struct alignas(16) PackedVertex
{
    float32 position_x,
            position_y,
            position_z,
            normal_x,
            normal_y,
            normal_z,
            texcoord0_x,
            texcoord0_y;
};

static_assert(sizeof(PackedVertex) == sizeof(float32) * 8);

using PackedIndex = uint32;

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

enum class StencilMode : uint32
{
    NONE,
    FILL,
    OUTLINE
};

enum class BlendMode : uint32
{
    NONE,
    NORMAL,
    ADDITIVE
};

struct StencilState
{
    uint id = 0;
    StencilMode mode = StencilMode::NONE;

    HYP_DEF_STRUCT_COMPARE_EQL(StencilState);

    bool operator<(const StencilState &other) const
    {
        return std::tie(id, mode) < std::tie(other.id, other.mode);
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(id);
        hc.Add(mode);

        return hc;
    }
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

using ImageSubResourceFlagBits = uint;

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
};

template <class T>
struct alignas(8) ShaderVec2
{
    union {
        struct { T x, y; };
        T values[2];
    };

    ShaderVec2() = default;
    ShaderVec2(const ShaderVec2 &other) = default;
    ShaderVec2(T x, T y) : x(x), y(y) {}
    ShaderVec2(const Extent2D &extent)
        : x(extent.width),
          y(extent.height)
    {
    }

    ShaderVec2(const Vec2<T> &xy)
        : x(xy.x),
          y(xy.y)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vec2<T>() const { return Vec2<T>(x, y); }
};

static_assert(sizeof(ShaderVec2<float>) == 8);
static_assert(sizeof(ShaderVec2<uint32>) == 8);

// shader vec3 is same size as vec4
template <class T>
struct alignas(16) ShaderVec3
{
    union {
        struct { T x, y, z, _w; };
        T values[4];
    };

    ShaderVec3() = default;
    ShaderVec3(const ShaderVec3 &other) = default;
    ShaderVec3(T x, T y, T z) : x(x), y(y), z(z) {}

    ShaderVec3(const Extent3D &extent)
        : x(extent.width),
          y(extent.height),
          z(extent.depth)
    {
    }

    ShaderVec3(const Vec3<T> &xyz)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vector3() const { return Vector3(x, y, z); }
};

static_assert(sizeof(ShaderVec3<float>)  == 16);
static_assert(sizeof(ShaderVec3<uint32>) == 16);

template <class T>
struct alignas(16) ShaderVec4
{
    union {
        struct { T x, y, z, w; };
        T values[4];
    };

    ShaderVec4() = default;
    ShaderVec4(const ShaderVec4 &other) = default;
    ShaderVec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    ShaderVec4(const Vector4 &vec)
        : x(T(vec.x)),
          y(T(vec.y)),
          z(T(vec.z)),
          w(T(vec.w))
    {
    }

    ShaderVec4(const Vec3<T> &xyz, T w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vector4() const { return Vector4(float(x), float(y), float(z), float(w)); }
};

static_assert(sizeof(ShaderVec4<float>) == 16);
static_assert(sizeof(ShaderVec4<uint32>) == 16);

struct alignas(16) ShaderMat4
{
    union {
        struct {
            float m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33;
        };

        float values[16];
        ShaderVec4<float> rows[4];
    };

    ShaderMat4() = default;
    ShaderMat4(const ShaderMat4 &other) = default;
    ShaderMat4(const Matrix4 &mat)
        : m00(mat[0][0]), m01(mat[0][1]), m02(mat[0][2]), m03(mat[0][3]),
          m10(mat[1][0]), m11(mat[1][1]), m12(mat[1][2]), m13(mat[1][3]),
          m20(mat[2][0]), m21(mat[2][1]), m22(mat[2][2]), m23(mat[2][3]),
          m30(mat[3][0]), m31(mat[3][1]), m32(mat[3][2]), m33(mat[3][3])
    {
    }

    constexpr ShaderVec4<float> &operator[](uint index) { return rows[index]; }
    constexpr const ShaderVec4<float> &operator[](uint index) const { return rows[index]; }

    operator Matrix4() const { return Matrix4(&values[0]); }
};

static_assert(sizeof(ShaderMat4) == 64);

struct alignas(8) Rect
{
    uint32 x0, y0,
        x1, y1;
};

static_assert(sizeof(Rect) == 16);


template <SizeType N, class T>
struct PaddedStructValue
{
    alignas(T) ubyte bytes[sizeof(T) + N];
};

template <class T, SizeType Size>
struct ShaderValue : public PaddedStructValue<Size - sizeof(T), T>
{
    static_assert(sizeof(T) <= Size, "T does not fit into required size!");

    ShaderValue()
    {
        new (this->bytes) T();
    }

    ShaderValue(const ShaderValue &other)
    {
        new (this->bytes) T(other.Get());
    }

    ShaderValue &operator=(const ShaderValue &other)
    {
        Get().~T();
        new (this->bytes) T(other.Get());

        return *this;
    }

    ShaderValue(const T &value)
    {
        new (this->bytes) T(value);
    }

    ShaderValue &operator=(const T &value)
    {
        Get().~T();
        new (this->bytes) T(value);

        return *this;
    }

    ShaderValue(ShaderValue &&other) noexcept
    {
        new (this->bytes) T(std::move(other.Get()));
    }

    ShaderValue &operator=(ShaderValue &&other) noexcept
    {
        Get().~T();
        new (this->bytes) T(std::move(other.Get()));

        return *this;
    }

    ShaderValue(T &&value) noexcept
    {
        new (this->bytes) T(std::move(value));
    }

    ShaderValue &operator=(T &&value) noexcept
    {
        Get().~T();
        new (this->bytes) T(std::move(value));

        return *this;
    }

    ~ShaderValue()
    {
        Get().~T();
    }

    T &Get()
    {
        return *reinterpret_cast<T *>(this->bytes);
    }

    const T &Get() const
    {
        return *reinterpret_cast<const T *>(this->bytes);
    }
};

template<class ...Args>
class PerFrameData
{
    struct FrameDataWrapper
    {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(uint32 num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    HYP_FORCE_INLINE uint NumFrames() const
        { return m_num_frames; }

    HYP_FORCE_INLINE FrameDataWrapper &operator[](uint32 index)
        { return m_data[index]; }

    HYP_FORCE_INLINE const FrameDataWrapper &operator[](uint32 index) const
        { return m_data[index]; }

    HYP_FORCE_INLINE FrameDataWrapper &At(uint32 index)
        { return m_data[index]; }

    HYP_FORCE_INLINE const FrameDataWrapper &At(uint32 index) const
        { return m_data[index]; }

    HYP_FORCE_INLINE void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    uint m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

} // namespace renderer
} // namespace hyperion

namespace std {

template <>
struct hash<hyperion::renderer::ImageSubResource>
{
    size_t operator()(const hyperion::renderer::ImageSubResource &sub_resource) const
    {
        ::hyperion::HashCode hc;
        hc.Add(sub_resource.flags);
        hc.Add(sub_resource.base_array_layer);
        hc.Add(sub_resource.num_layers);
        hc.Add(sub_resource.base_mip_level);
        hc.Add(sub_resource.num_levels);

        return hc.Value();
    }
};

} // namespace std

#endif
