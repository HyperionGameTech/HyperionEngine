#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Frustum Reflection Data

HYP_BEGIN_STRUCT(Frustum, 246, 0, {}, HypClassAttribute("size", 224),HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Planes)), &Frustum::planes, offsetof(Frustum, planes)),
    HypField(NAME(HYP_STR(Corners)), &Frustum::corners, offsetof(Frustum, corners))
HYP_END_STRUCT

#pragma endregion Frustum Reflection Data

static_assert(sizeof(Frustum) == 224, "Expected sizeof(Frustum) to be 224 bytes");
} // namespace hyperion

