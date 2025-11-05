#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Triangle Reflection Data

HYP_BEGIN_STRUCT(Triangle, 242, 0, {}, ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(Points)), &Triangle::points, offsetof(Triangle, points))
HYP_END_STRUCT

#pragma endregion Triangle Reflection Data

} // namespace hyperion

