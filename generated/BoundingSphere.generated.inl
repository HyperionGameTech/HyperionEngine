#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region BoundingSphere Reflection Data

HYP_BEGIN_STRUCT(BoundingSphere, 244, 0, {}, ClassAttribute("size", 32))
    Field(NAME(HYP_STR(Center)), &BoundingSphere::center, offsetof(BoundingSphere, center), Span<const ClassAttribute> { {ClassAttribute("property", "Center"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Radius)), &BoundingSphere::radius, offsetof(BoundingSphere, radius), Span<const ClassAttribute> { {ClassAttribute("property", "Radius"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion BoundingSphere Reflection Data

static_assert(sizeof(BoundingSphere) == 32, "Expected sizeof(BoundingSphere) to be 32 bytes");
} // namespace hyperion

