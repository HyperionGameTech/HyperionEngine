#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region PhysicsMaterial Reflection Data

HYP_BEGIN_STRUCT(PhysicsMaterial, 263, 0, {})
    HypField(NAME(HYP_STR(Mass)), &PhysicsMaterial::mass, offsetof(PhysicsMaterial, mass), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Mass") } })
HYP_END_STRUCT

#pragma endregion PhysicsMaterial Reflection Data

} // namespace hyperion

