/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Vertex.hpp>
#include <core/math/Transform.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/Tuple.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/reflection/ObjectMacros.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace Hyperion {

struct BoundingBox;
struct Triangle;
class RayTestResults;
struct RayHit;
class Mat4f;

using RayHitID = uint32;

HYP_ENUM()
enum RayTestFlags : uint32
{
    RTF_NONE = 0x0,

    RTF_USE_BVH = 0x1,
    RTF_EDITOR_PICK = 0x2,

    RTF_MAX = 0xFFFFFFFFu
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
    bool TestAABB(const BoundingBox& aabb, RayHitID hitId, const void* userData, RayTestResults& outResults) const;

    Optional<RayHit> TestPlane(const Vec3f& position, const Vec3f& normal) const;
    bool TestPlane(const Vec3f& position, const Vec3f& normal, RayTestResults& outResults) const;
    bool TestPlane(const Vec3f& position, const Vec3f& normal, RayHitID hitId, RayTestResults& outResults) const;
    bool TestPlane(const Vec3f& position, const Vec3f& normal, RayHitID hitId, const void* userData, RayTestResults& outResults) const;

    Optional<RayHit> TestTriangle(const Triangle& triangle) const;
    bool TestTriangle(const Triangle& triangle, RayTestResults& outResults) const;
    bool TestTriangle(const Triangle& triangle, RayHitID hitId, RayTestResults& outResults) const;
    bool TestTriangle(const Triangle& triangle, RayHitID hitId, const void* userData, RayTestResults& outResults) const;

    Optional<RayHit> TestTriangleList(
        const Array<Vertex>& vertices,
        const Array<uint32>& indices,
        const Transform& transform) const;

    Optional<RayHit> TestTriangleList(
        const Span<Triangle>& triangles,
        const Transform& transform) const;

    bool TestTriangleList(
        const Array<Vertex>& vertices,
        const Array<uint32>& indices,
        const Transform& transform,
        RayTestResults& outResults) const;

    bool TestTriangleList(
        const Span<Triangle>& triangles,
        const Transform& transform,
        RayTestResults& outResults) const;

    bool TestTriangleList(
        const Array<Vertex>& vertices,
        const Array<uint32>& indices,
        const Transform& transform,
        RayHitID hitId,
        RayTestResults& outResults) const;

    bool TestTriangleList(
        const Span<Triangle>& triangles,
        const Transform& transform,
        RayHitID hitId,
        RayTestResults& outResults) const;

    bool TestTriangleList(
        const Array<Vertex>& vertices,
        const Array<uint32>& indices,
        const Transform& transform,
        RayHitID hitId,
        const void* userData,
        RayTestResults& outResults) const;

    bool TestTriangleList(
        const Span<Triangle>& triangles,
        const Transform& transform,
        RayHitID hitId,
        const void* userData,
        RayTestResults& outResults) const;

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
    const void* userData = nullptr;

    bool operator<(const RayHit& other) const
    {
        return Tie(
                   distance,
                   hitpoint,
                   normal,
                   barycentricCoords,
                   id,
                   userData)
            < Tie(
                other.distance,
                other.hitpoint,
                other.normal,
                other.barycentricCoords,
                other.id,
                other.userData);
    }

    bool operator==(const RayHit& other) const
    {
        return distance == other.distance
            && hitpoint == other.hitpoint
            && normal == other.normal
            && barycentricCoords == other.barycentricCoords
            && id == other.id
            && userData == other.userData;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(distance);
        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(barycentricCoords.GetHashCode());
        hc.Add(id);
        hc.Add(UIntPtr(userData));

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
