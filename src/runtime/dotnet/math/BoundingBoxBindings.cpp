#include <math/BoundingBox.hpp>
#include <runtime/dotnet/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    float BoundingBox_GetRadius(ManagedBoundingBox managed_bounding_box)
    {
        BoundingBox b(managed_bounding_box);
        return b.GetRadius();
    }

    bool BoundingBox_Contains(ManagedBoundingBox managed_bounding_box, ManagedBoundingBox other)
    {
        BoundingBox b(managed_bounding_box);
        return b.Contains(other);
    }

    bool BoundingBox_ContainsPoint(ManagedBoundingBox managed_bounding_box, ManagedVec3f point)
    {
        BoundingBox b(managed_bounding_box);
        return b.ContainsPoint(point);
    }
}