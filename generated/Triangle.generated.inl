#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Triangle Reflection Data

HYP_BEGIN_STRUCT(Triangle, 241, 0, {}, HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Points)), &Triangle::points, offsetof(Triangle, points))
HYP_END_STRUCT

#pragma endregion Triangle Reflection Data

} // namespace hyperion

