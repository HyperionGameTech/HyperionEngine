#ifndef HYPERION_V2_BACKEND_RENDERER_STRUCTS_H
#define HYPERION_V2_BACKEND_RENDERER_STRUCTS_H

#include <util/Defines.hpp>

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

static_assert(sizeof(ShaderVec3<float>)  == 16);
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

struct alignas(4) ShaderMat4
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

} // namespace renderer
} // namespace hyperion

#endif