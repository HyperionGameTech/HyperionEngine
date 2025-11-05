#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Frustum Reflection Data

HYP_BEGIN_STRUCT(Frustum, 247, 0, {}, ClassAttribute("size", 224),ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(Planes)), &Frustum::planes, offsetof(Frustum, planes)),
    Field(NAME(HYP_STR(Corners)), &Frustum::corners, offsetof(Frustum, corners))
HYP_END_STRUCT

#pragma endregion Frustum Reflection Data

static_assert(sizeof(Frustum) == 224, "Expected sizeof(Frustum) to be 224 bytes");
} // namespace hyperion

