#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Transform Reflection Data

HYP_BEGIN_STRUCT(Transform, 239, 0, {}, HypClassAttribute("size", 112),HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Translation)), &Transform::translation, offsetof(Transform, translation)),
    HypField(NAME(HYP_STR(Scale)), &Transform::scale, offsetof(Transform, scale)),
    HypField(NAME(HYP_STR(Rotation)), &Transform::rotation, offsetof(Transform, rotation)),
    HypField(NAME(HYP_STR(Matrix)), &Transform::matrix, offsetof(Transform, matrix), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion Transform Reflection Data

static_assert(sizeof(Transform) == 112, "Expected sizeof(Transform) to be 112 bytes");
} // namespace hyperion

