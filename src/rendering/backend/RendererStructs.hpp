#ifndef HYPERION_V2_BACKEND_RENDERER_STRUCTS_H
#define HYPERION_V2_BACKEND_RENDERER_STRUCTS_H

#include <core/lib/String.hpp>
#include <util/Defines.hpp>
#include <util/EnumOptions.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <tuple>

namespace hyperion::renderer {

enum class DatumType : UInt32
{
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class FaceCullMode : UInt32
{
    NONE,
    BACK,
    FRONT
};

enum class FillMode : UInt32
{
    FILL,
    LINE
};

enum class Topology : UInt32
{
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

enum class StencilMode : UInt32
{
    NONE,
    FILL,
    OUTLINE
};

enum class BlendMode : UInt32
{
    NONE,
    NORMAL,
    ADDITIVE
};

struct StencilState
{
    UInt id = 0;
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

struct VertexAttribute
{
    enum Type : UInt32
    {
        MESH_INPUT_ATTRIBUTE_UNDEFINED    = 0x0,
        MESH_INPUT_ATTRIBUTE_POSITION     = 0x1,
        MESH_INPUT_ATTRIBUTE_NORMAL       = 0x2,
        MESH_INPUT_ATTRIBUTE_TEXCOORD0    = 0x4,
        MESH_INPUT_ATTRIBUTE_TEXCOORD1    = 0x8,
        MESH_INPUT_ATTRIBUTE_TANGENT      = 0x10,
        MESH_INPUT_ATTRIBUTE_BITANGENT    = 0x20,
        MESH_INPUT_ATTRIBUTE_BONE_INDICES = 0x40,
        MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS = 0x80,
    };

    static const EnumOptions<Type, VertexAttribute, 16> mapping;

    const char *name;
    UInt32 location;
    UInt32 binding;
    SizeType size; // total size -- num elements * sizeof(float)

    bool operator<(const VertexAttribute &other) const
        { return location < other.location; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(String(name));
        hc.Add(location);
        hc.Add(binding);
        hc.Add(size);

        return hc;
    }
};

struct VertexAttributeSet
{
    UInt64 flag_mask;

    constexpr VertexAttributeSet()
        : flag_mask(0) {}

    constexpr VertexAttributeSet(UInt64 flag_mask)
        : flag_mask(flag_mask) {}

    constexpr VertexAttributeSet(VertexAttribute::Type flags)
        : flag_mask(UInt64(flags)) {}

    constexpr VertexAttributeSet(const VertexAttributeSet &other)
        : flag_mask(other.flag_mask) {}

    VertexAttributeSet &operator=(const VertexAttributeSet &other)
    {
        flag_mask = other.flag_mask;

        return *this;
    }

    ~VertexAttributeSet() = default;

    explicit operator bool() const { return flag_mask != 0; }

    bool operator==(const VertexAttributeSet &other) const
        { return flag_mask == other.flag_mask; }

    bool operator!=(const VertexAttributeSet &other) const
        { return flag_mask != other.flag_mask; }

    bool operator==(UInt64 flags) const
        { return flag_mask == flags; }

    bool operator!=(UInt64 flags) const
        { return flag_mask != flags; }

    VertexAttributeSet operator~() const
        { return ~flag_mask; }

    VertexAttributeSet operator&(const VertexAttributeSet &other) const
        { return { flag_mask & other.flag_mask }; }

    VertexAttributeSet &operator&=(const VertexAttributeSet &other)
        { flag_mask &= other.flag_mask; return *this; }

    VertexAttributeSet operator&(UInt64 flags) const
        { return { flag_mask & flags }; }

    VertexAttributeSet &operator&=(UInt64 flags)
        { flag_mask &= flags; return *this; }
    
    VertexAttributeSet operator|(const VertexAttributeSet &other) const
        { return { flag_mask | other.flag_mask }; }

    VertexAttributeSet &operator|=(const VertexAttributeSet &other)
        { flag_mask |= other.flag_mask; return *this; }

    VertexAttributeSet operator|(UInt64 flags) const
        { return { flag_mask | flags }; }

    VertexAttributeSet &operator|=(UInt64 flags)
        { flag_mask |= flags; return *this; }

    bool operator<(const VertexAttributeSet &other) const { return flag_mask < other.flag_mask; }

    bool Has(VertexAttribute::Type type) const { return bool(operator&(UInt64(type))); }

    void Set(UInt64 flags, bool enable = true)
    {
        if (enable) {
            flag_mask |= flags;
        } else {
            flag_mask &= ~flags;
        }
    }

    void Set(VertexAttribute::Type type, bool enable = true)
    {
        Set(UInt64(type), enable);
    }

    void Merge(const VertexAttributeSet &other)
    {
        flag_mask |= other.flag_mask;
    }

    std::vector<VertexAttribute> BuildAttributes() const
    {
        std::vector<VertexAttribute> attributes;
        attributes.reserve(VertexAttribute::mapping.Size());

        for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                attributes.push_back(VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)]);
            }
        }

        return attributes;
    }

    SizeType CalculateVertexSize() const
    {
        SizeType size = 0;

        for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
            const UInt64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                size += VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)].size;
            }
        }

        return size;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flag_mask);

        return hc;
    }
};

constexpr VertexAttributeSet static_mesh_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT
);

constexpr VertexAttributeSet skeleton_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES
);

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
    ShaderVec2(const Vector2 &vec)
        : x(vec.x),
          y(vec.y)
    {
    }
    ShaderVec2(const Extent2D &extent)
        : x(extent.width),
          y(extent.height)
    {
    }

    constexpr T &operator[](UInt index) { return values[index]; }
    constexpr const T &operator[](UInt index) const { return values[index]; }

    operator Vector2() const { return Vector2(x, y); }
};

static_assert(sizeof(ShaderVec2<Float>) == 8);
static_assert(sizeof(ShaderVec2<UInt32>) == 8);

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
    ShaderVec3(const Vector3 &vec)
        : x(vec.x),
          y(vec.y),
          z(vec.z)
    {
    }
    ShaderVec3(const Extent3D &extent)
        : x(extent.width),
          y(extent.height),
          z(extent.depth)
    {
    }

    constexpr T &operator[](UInt index) { return values[index]; }
    constexpr const T &operator[](UInt index) const { return values[index]; }

    operator Vector3() const { return Vector3(x, y, z); }
};

static_assert(sizeof(ShaderVec3<Float>)  == 16);
static_assert(sizeof(ShaderVec3<UInt32>) == 16);

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
        : x(vec.x),
          y(vec.y),
          z(vec.z),
          w(vec.w)
    {
    }

    constexpr T &operator[](UInt index) { return values[index]; }
    constexpr const T &operator[](UInt index) const { return values[index]; }

    operator Vector4() const { return Vector4(x, y, z, w); }
};

static_assert(sizeof(ShaderVec4<Float>)  == 16);
static_assert(sizeof(ShaderVec4<UInt32>) == 16);

struct alignas(16) ShaderMat4
{
    union {
        struct {
            Float m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33;
        };

        Float values[16];
        ShaderVec4<Float> rows[4];
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

    constexpr ShaderVec4<Float> &operator[](UInt index) { return rows[index]; }
    constexpr const ShaderVec4<Float> &operator[](UInt index) const { return rows[index]; }

    operator Matrix4() const { return Matrix4(&values[0]); }
};

static_assert(sizeof(ShaderMat4) == 64);

struct alignas(8) Rect
{
    UInt32 x0, y0,
        x1, y1;
};

static_assert(sizeof(Rect) == 16);


template <SizeType N, class T>
struct PaddedStructValue
{
    alignas(T) UByte bytes[sizeof(T) + N];
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

} // namespace renderer
} // namespace hyperion

#endif