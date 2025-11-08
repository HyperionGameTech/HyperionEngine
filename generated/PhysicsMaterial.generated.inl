#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region PhysicsMaterial Reflection Data

HYP_BEGIN_STRUCT(PhysicsMaterial, 364, 0, {})
    Field(NAME(HYP_STR(Mass)), &PhysicsMaterial::mass, offsetof(PhysicsMaterial, mass), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Mass") } })
HYP_END_STRUCT

#pragma endregion PhysicsMaterial Reflection Data

} // namespace hyperion

