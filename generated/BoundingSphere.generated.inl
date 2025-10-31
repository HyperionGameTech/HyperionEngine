#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region BoundingSphere Reflection Data

HYP_BEGIN_STRUCT(BoundingSphere, 243, 0, {}, HypClassAttribute("size", 32))
    HypField(NAME(HYP_STR(Center)), &BoundingSphere::center, offsetof(BoundingSphere, center), Span<const HypClassAttribute> { {HypClassAttribute("property", "Center"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Radius)), &BoundingSphere::radius, offsetof(BoundingSphere, radius), Span<const HypClassAttribute> { {HypClassAttribute("property", "Radius"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion BoundingSphere Reflection Data

static_assert(sizeof(BoundingSphere) == 32, "Expected sizeof(BoundingSphere) to be 32 bytes");
} // namespace hyperion

