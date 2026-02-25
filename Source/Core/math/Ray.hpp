/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>
#include <Core/math/Transform.hpp>

#include <Core/containers/FlatSet.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Tuple.hpp>
#include <Core/utilities/Span.hpp>
#include <Core/utilities/EnumFlags.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct BoundingBox;
struct Triangle;
class RayTestResults;
struct RayHit;
class Mat4f;

using RayHitID = uint32;

HYP_ENUM()
enum class RayTestFlags : uint32
{
    None = 0x0,

    TestBVH = 0x1,
    EditorPick = 0x2,

    Max = 0xFFFFFFFFu
};

HYP_MAKE_ENUM_FLAGS(RayTestFlags)

HYP_STRUCT(Size = 32, Serialize = "bitwise")
struct HYP_API Ray
{
    HYP_STRUCT_BODY(Ray);

    HYP_FIELD(Property = "Position")
    Vec3f position;

    HYP_FIELD(Property = "Direction")
    Vec3f direction;

    HYP_FORCE_INLINE bool operator==(const Ray& other) const
    {
        return position == other.position
            && direction == other.direction;
    }

    HYP_FORCE_INLINE bool operator!=(const Ray& other) const
    {
        return position != other.position
            || direction != other.direction;
    }

    Ray operator*(const Mat4f& transform) const;

    Optional<RayHit> TestAABB(const BoundingBox& aabb) const;
    bool TestAABB(const BoundingBox& aabb, RayTestResults& outResults) const;
    bool TestAABB(const BoundingBox& aabb, RayHitID hitId, RayTestResults& outResults) const;

    Optional<RayHit> TestPlane(const Vec3f& position, const Vec3f& normal) const;
    bool TestPlane(const Vec3f& position, const Vec3f& normal, RayTestResults& outResults) const;
    bool TestPlane(const Vec3f& position, const Vec3f& normal, RayHitID hitId, RayTestResults& outResults) const;

    Optional<RayHit> TestTriangle(const Triangle& triangle) const;
    bool TestTriangle(const Triangle& triangle, RayTestResults& outResults) const;
    bool TestTriangle(const Triangle& triangle, RayHitID hitId, RayTestResults& outResults) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(direction.GetHashCode());

        return hc;
    }
};

HYP_API Ray operator*(const Mat4f& transform, const Ray& ray);

struct RayHit
{
    Vec3f hitpoint;
    Vec3f normal;
    Vec3f barycentricCoords;
    float distance = 0.0f;
    RayHitID id = ~0u;
    class Node* node = nullptr;

    bool operator<(const RayHit& other) const
    {
        return distance < other.distance;
    }

    bool operator==(const RayHit& other) const
    {
        return distance == other.distance
            && hitpoint == other.hitpoint
            && normal == other.normal
            && barycentricCoords == other.barycentricCoords
            && id == other.id;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(distance);
        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(barycentricCoords.GetHashCode());
        hc.Add(id);

        return hc;
    }
};

class RayTestResults : public FlatSet<RayHit>
{
public:
    bool AddHit(const RayHit& hit)
    {
        return Insert(hit).second;
    }
};

} // namespace Hyperion
