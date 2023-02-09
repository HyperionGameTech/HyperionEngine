#ifndef HYPERION_MATH_EXTENT_HPP
#define HYPERION_MATH_EXTENT_HPP

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>

#include <Types.hpp>

namespace hyperion {

struct Extent2D
{
    union
    {
        struct // NOLINT(clang-diagnostic-nested-anon-types)
        { 
            UInt32 width, height;
        };

        UInt32 v[2];
    };

    constexpr Extent2D()
        : width(0),
          height(0)
    {
    }

    constexpr Extent2D(UInt32 width, UInt32 height)
        : width(width),
          height(height)
    {
    }

    constexpr Extent2D(const Vec2u &vec)
        : width(vec.x),
          height(vec.y)
    {
    }

    constexpr Extent2D(const Vec2i &vec)
        : width(UInt32(vec.x)),
          height(UInt32(vec.y))
    {
    }

    Extent2D(const Extent2D &other) = default;
    Extent2D &operator=(const Extent2D &other) = default;
    Extent2D(Extent2D &&other) noexcept = default;
    Extent2D &operator=(Extent2D &&other) noexcept = default;
    ~Extent2D() = default;

    constexpr bool operator==(const Extent2D &other) const
    {
        return width == other.width
            && height == other.height;
    }

    constexpr bool operator!=(const Extent2D &other) const
        { return !operator==(other); }
    
    constexpr Extent2D operator*(const Extent2D &other) const
        { return Extent2D(width * other.width, height * other.height); }

    Extent2D &operator*=(const Extent2D &other)
    {
        width *= other.width;
        height *= other.height;

        return *this;
    }
    
    constexpr Extent2D operator*(UInt32 scalar) const
        { return Extent2D(width * scalar, height * scalar); }

    Extent2D &operator*=(UInt32 scalar)
    {
        width *= scalar;
        height *= scalar;

        return *this;
    }

    Extent2D operator/(const Extent2D &other) const
    {
        AssertThrow(other.width != 0 && other.height != 0);
        return Extent2D(width / other.width, height / other.height);
    }

    Extent2D &operator/=(const Extent2D &other)
    {
        AssertThrow(other.width != 0 && other.height != 0);

        width  /= other.width;
        height /= other.height;

        return *this;
    }
    
    Extent2D operator/(UInt32 scalar) const
    {
        AssertThrow(scalar != 0);

        return Extent2D(width / scalar, height / scalar);
    }

    Extent2D &operator/=(UInt32 scalar)
    {
        AssertThrow(scalar != 0);

        width /= scalar;
        height /= scalar;

        return *this;
    }
    
    constexpr UInt32 &operator[](UInt32 index)
        { return v[index]; }

    constexpr UInt32 operator[](UInt32 index) const
        { return v[index]; }

    SizeType Size() const
        { return static_cast<SizeType>(width) * static_cast<SizeType>(height); }

    constexpr operator Vec2u() const
    {
        return {
            width,
            height
        };
    }

    constexpr explicit operator Vec2i() const
    {
        return {
            Vec2i::Type(width),
            Vec2i::Type(height)
        };
    }

    explicit operator Vector2() const
    {
        return {
            Float(width),
            Float(height)
        };
    }
};

static_assert(sizeof(Extent2D) == 8);

struct Extent3D
{
    union
    {
        struct // NOLINT(clang-diagnostic-nested-anon-types)
        {
            UInt32 width, height, depth, _pad;
        };

        UInt32 v[4];
    };

    constexpr Extent3D() // NOLINT(cppcoreguidelines-pro-type-member-init)
        : width(0),
          height(0),
          depth(0),
          _pad(0)
    {
    }

    explicit constexpr Extent3D(UInt32 extent)  // NOLINT(cppcoreguidelines-pro-type-member-init)
        : width(extent),
          height(extent),
          depth(extent),
          _pad(0)
    {
    }

    constexpr Extent3D(UInt32 width, UInt32 height, UInt32 depth) // NOLINT(cppcoreguidelines-pro-type-member-init)
        : width(width),
          height(height),
          depth(depth),
          _pad(0)
    {
    }

    explicit Extent3D(const Vector3 &extent) // NOLINT(cppcoreguidelines-pro-type-member-init)
        : width(UInt32(extent.x)),
          height(UInt32(extent.y)),
          depth(UInt32(extent.z))
    {
    }

    constexpr Extent3D(const Vec3u &vec)
        : width(vec.x),
          height(vec.y),
          depth(vec.z)
    {
    }

    constexpr Extent3D(const Vec3i &vec)
        : width(UInt32(vec.x)),
          height(UInt32(vec.y)),
          depth(UInt32(vec.z))
    {
    }

    explicit Extent3D(const Extent2D &extent_2d, UInt32 depth = 1) // NOLINT(cppcoreguidelines-pro-type-member-init)
        : width(extent_2d.width),
          height(extent_2d.height),
          depth(depth)
    {
    }

    Extent3D(const Extent3D &other) = default;
    Extent3D &operator=(const Extent3D &other) = default;
    Extent3D(Extent3D &&other) noexcept = default;
    Extent3D &operator=(Extent3D &&other) noexcept = default;
    ~Extent3D() = default;

    constexpr bool operator==(const Extent3D &other) const
    {
        return width == other.width
            && height == other.height
            && depth == other.depth;
    }

    constexpr bool operator!=(const Extent3D &other) const
        { return !operator==(other); }
    
    constexpr Extent3D operator*(const Extent3D &other) const
    {
        return {
            width * other.width,
            height * other.height,
            depth * other.depth
        };
    }

    Extent3D &operator*=(const Extent3D &other)
    {
        width *= other.width;
        height *= other.height;
        depth *= other.depth;

        return *this;
    }
    
    constexpr Extent3D operator*(UInt32 scalar) const
    {
        return {
            width * scalar,
            height * scalar,
            depth * scalar
        };
    }

    Extent3D &operator*=(UInt32 scalar)
    {
        width *= scalar;
        height *= scalar;
        depth *= scalar;

        return *this;
    }

    Extent3D operator/(const Extent3D &other) const
    {
        AssertThrow(other.width != 0 && other.height != 0 && other.depth != 0);
        return {
            width / other.width,
            height / other.height,
            depth / other.depth
        };
    }

    Extent3D &operator/=(const Extent3D &other)
    {
        AssertThrow(other.width != 0 && other.height != 0 && other.depth != 0);

        width /= other.width;
        height /= other.height;
        depth /= other.depth;

        return *this;
    }
    
    Extent3D operator/(UInt32 scalar) const
    {
        AssertThrow(scalar != 0);

        return {
            width / scalar,
            height / scalar,
            depth / scalar
        };
    }

    Extent3D &operator/=(UInt32 scalar)
    {
        AssertThrow(scalar != 0);

        width /= scalar;
        height /= scalar;
        depth /= scalar;

        return *this;
    }
    
    constexpr UInt32 &operator[](UInt32 index) { return v[index]; }
    constexpr UInt32 operator[](UInt32 index) const { return v[index]; }

    constexpr explicit operator Extent2D() const
    {
        return {
            width,
            height
        };
    }
    
    constexpr operator Vec3u() const
    {
        return {
            width,
            height,
            depth
        };
    }

    constexpr explicit operator Vec3i() const
    {
        return {
            Vec3i::Type(width),
            Vec3i::Type(height),
            Vec3i::Type(depth)
        };
    }

    explicit operator Vector3() const
    {
        return {
            Float(width),
            Float(height),
            Float(depth)
        };
    }

    constexpr SizeType Size() const
        { return SizeType(width) * SizeType(height) * SizeType(depth); }
};

static_assert(sizeof(Extent3D) == 16);

} //  namespace hyperion

#endif