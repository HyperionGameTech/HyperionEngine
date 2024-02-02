#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_MATH_TYPES_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_MATH_TYPES_HPP

#include <math/Vector3.hpp>
#include <math/Matrix4.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>

#include <core/lib/CMemory.hpp>

#include <type_traits>

namespace hyperion::v2 {

extern "C" struct ManagedVec3f
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

extern "C" struct ManagedQuaternion
{
    float x;
    float y;
    float z;
    float w;

    ManagedQuaternion() = default;

    ManagedQuaternion(const Quaternion &q)
        : x(q.x), y(q.y), z(q.z), w(q.w)
    {
    }

    operator Quaternion() const
        { return Quaternion { x, y, z, w }; }
};

static_assert(std::is_trivial_v<ManagedQuaternion>, "ManagedQuaternion must be a trivial type to be used in C#");
static_assert(sizeof(ManagedQuaternion) == 16, "ManagedQuaternion must be 16 bytes to be used in C#");

extern "C" struct ManagedMatrix4
{
    float values[16];

    ManagedMatrix4() = default;

    ManagedMatrix4(const Matrix4 &m)
    {
        Memory::MemCpy(values, m.values, sizeof(values));
    }

    operator Matrix4() const
    {
        Matrix4 m;
        Memory::MemCpy(m.values, values, sizeof(values));
        return m;
    }
};

extern "C" struct ManagedTransform
{
    ManagedVec3f        translation;
    ManagedVec3f        scale;
    ManagedQuaternion   rotation;
    ManagedMatrix4      matrix;

    ManagedTransform() = default;

    ManagedTransform(const Transform &t)
        : translation(t.GetTranslation()),
          rotation(t.GetRotation()),
          scale(t.GetScale()),
          matrix(t.GetMatrix())
    {
    }

    operator Transform() const
        { return Transform { translation, scale, rotation }; }
};

static_assert(std::is_trivial_v<ManagedTransform>, "ManagedTransform must be a trivial type to be used in C#");
static_assert(sizeof(ManagedTransform) == 112, "ManagedTransform must be 64 bytes to be used in C#");

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