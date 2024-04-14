#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_MATH_TYPES_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_MATH_TYPES_HPP

#include <math/Vector3.hpp>
#include <math/Matrix4.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>

#include <core/lib/Memory.hpp>

#include <type_traits>

namespace hyperion::v2 {

extern "C" struct alignas(16) ManagedVec3f
{
    float x;
    float y;
    float z;
    float _pad;

    ManagedVec3f() = default;

    ManagedVec3f(const Vec3f &v)
        : x(v.x), y(v.y), z(v.z), _pad(0.0f)
    {
    }

    operator Vec3f() const
        { return Vec3f { x, y, z }; }
};

static_assert(std::is_trivial_v<ManagedVec3f>, "ManagedVec3f must be a trivial type to be used in C#");
static_assert(sizeof(ManagedVec3f) == 16, "ManagedVec3f must be 16 bytes to be used in C#");
static_assert(alignof(ManagedVec3f) == 16, "ManagedVec3f must be 16-byte aligned to be used in C#");

extern "C" struct alignas(16) ManagedVec3i
{
    int32 x;
    int32 y;
    int32 z;
    int32 _pad;

    ManagedVec3i() = default;

    ManagedVec3i(const Vec3i &v)
        : x(v.x), y(v.y), z(v.z), _pad(0)
    {
    }

    operator Vec3i() const
        { return Vec3i { x, y, z }; }
};

static_assert(std::is_trivial_v<ManagedVec3i>, "ManagedVec3i must be a trivial type to be used in C#");
static_assert(sizeof(ManagedVec3i) == 16, "ManagedVec3i must be 16 bytes to be used in C#");
static_assert(alignof(ManagedVec3i) == 16, "ManagedVec3i must be 16-byte aligned to be used in C#");

extern "C" struct alignas(16) ManagedVec3u
{
    uint32 x;
    uint32 y;
    uint32 z;
    uint32 _pad;

    ManagedVec3u() = default;

    ManagedVec3u(const Vec3u &v)
        : x(v.x), y(v.y), z(v.z), _pad(0)
    {
    }

    operator Vec3u() const
        { return Vec3u { x, y, z }; }
};

static_assert(std::is_trivial_v<ManagedVec3u>, "ManagedVec3u must be a trivial type to be used in C#");
static_assert(sizeof(ManagedVec3u) == 16, "ManagedVec3u must be 16 bytes to be used in C#");
static_assert(alignof(ManagedVec3u) == 16, "ManagedVec3u must be 16-byte aligned to be used in C#");

extern "C" struct ManagedBoundingBox
{
    ManagedVec3f min;
    ManagedVec3f max;

    ManagedBoundingBox() = default;

    ManagedBoundingBox(const BoundingBox &b)
        : min(b.GetMin()), max(b.GetMax())
    {
    }

    operator BoundingBox() const
        { return BoundingBox { min, max }; }
};

static_assert(std::is_trivial_v<ManagedBoundingBox>, "ManagedBoundingBox must be a trivial type to be used in C#");
static_assert(sizeof(ManagedBoundingBox) == 32, "ManagedBoundingBox must be 32 bytes to be used in C#");

} // namespace hyperion::v2

#endif