/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Containers/FlatSet.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Tuple.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

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

HYP_MAKE_ENUM_FLAGS(RayTestFlags);

HYP_STRUCT(Size = 32, Serialize = "bitwise")
struct CORE_API Ray
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

CORE_API Ray operator*(const Mat4f& transform, const Ray& ray);

struct RayHit
{
    RayHitID id = ~0u;
    float distance = 0.0f;
    
    Vec3f hitpoint;
    Vec3f normal;
    Vec3f barycentricCoords;

    uint32 triangleIndex = ~0u;
    class Node* node = nullptr;

    // Hit came from a bounding volume, not surface geometry. Reported at the point the ray enters
    // the volume, so it sorts after every exact hit regardless of distance.
    bool isApproximate = false;

    bool operator<(const RayHit& other) const
    {
        if (isApproximate != other.isApproximate)
        {
            return !isApproximate;
        }

        if (distance != other.distance)
        {
            return distance < other.distance;
        }

        if (triangleIndex != other.triangleIndex)
        {
            return triangleIndex < other.triangleIndex;
        }

        return id < other.id;
    }

    // Must compare the same fields operator< orders by: RayTestResults finds the insertion point
    // with operator< then rejects the value if operator== holds there.
    bool operator==(const RayHit& other) const
    {
        return isApproximate == other.isApproximate
            && distance == other.distance
            && triangleIndex == other.triangleIndex
            && id == other.id;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(isApproximate);
        hc.Add(distance);
        hc.Add(triangleIndex);
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
