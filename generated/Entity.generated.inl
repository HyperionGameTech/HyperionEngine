#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Entity Reflection Data

HYP_BEGIN_CLASS(Entity, 166, 12, NAME("Node"))
    Method(NAME(HYP_STR(SerializeComponents)), &Entity::SerializeComponents, Span<const ClassAttribute> { {ClassAttribute("property", "Components"), ClassAttribute("noscriptbindings", true) } }),
    Method(NAME(HYP_STR(DeserializeComponents)), &Entity::DeserializeComponents, Span<const ClassAttribute> { {ClassAttribute("property", "Components"), ClassAttribute("noscriptbindings", true), ClassAttribute("loadorder", 100) } })
HYP_END_CLASS

#pragma endregion Entity Reflection Data

} // namespace hyperion

