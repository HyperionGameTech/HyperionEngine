#include <math/BoundingBox.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT float BoundingBox_Intersects(BoundingBox *left, BoundingBox *right)
{
    return left->Intersects(*right);
}

HYP_EXPORT float BoundingBox_GetRadius(BoundingBox *bounding_box)
{
    return bounding_box->GetRadius();
}

HYP_EXPORT bool BoundingBox_Contains(BoundingBox *left, BoundingBox *right)
{
    return left->Contains(*right);
}

HYP_EXPORT bool BoundingBox_ContainsPoint(BoundingBox *bounding_box, Vec3f *point)
{
    return bounding_box->ContainsPoint(*point);
}
} // extern "C"