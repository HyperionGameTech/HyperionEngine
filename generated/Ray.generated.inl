#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region RayTestFlags Reflection Data

HYP_BEGIN_ENUM(RayTestFlags, 244, 0, {})
    HypConstant(NAME(HYP_STR(RTF_NONE)), RayTestFlags::RTF_NONE),
    HypConstant(NAME(HYP_STR(RTF_USE_BVH)), RayTestFlags::RTF_USE_BVH),
    HypConstant(NAME(HYP_STR(RTF_EDITOR_PICK)), RayTestFlags::RTF_EDITOR_PICK),
    HypConstant(NAME(HYP_STR(RTF_MAX)), RayTestFlags::RTF_MAX)
HYP_END_ENUM

#pragma endregion RayTestFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Ray Reflection Data

HYP_BEGIN_STRUCT(Ray, 245, 0, {}, HypClassAttribute("size", 32),HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Position)), &Ray::position, offsetof(Ray, position), Span<const HypClassAttribute> { {HypClassAttribute("property", "Position") } }),
    HypField(NAME(HYP_STR(Direction)), &Ray::direction, offsetof(Ray, direction), Span<const HypClassAttribute> { {HypClassAttribute("property", "Direction") } })
HYP_END_STRUCT

#pragma endregion Ray Reflection Data

static_assert(sizeof(Ray) == 32, "Expected sizeof(Ray) to be 32 bytes");
} // namespace hyperion

