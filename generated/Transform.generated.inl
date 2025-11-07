#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Transform Reflection Data

HYP_BEGIN_STRUCT(Transform, 247, 0, {}, ClassAttribute("size", 112),ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(Translation)), &Transform::translation, offsetof(Transform, translation)),
    Field(NAME(HYP_STR(Scale)), &Transform::scale, offsetof(Transform, scale)),
    Field(NAME(HYP_STR(Rotation)), &Transform::rotation, offsetof(Transform, rotation)),
    Field(NAME(HYP_STR(Matrix)), &Transform::matrix, offsetof(Transform, matrix), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion Transform Reflection Data

static_assert(sizeof(Transform) == 112, "Expected sizeof(Transform) to be 112 bytes");
} // namespace hyperion

