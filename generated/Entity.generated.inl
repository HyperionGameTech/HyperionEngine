#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Entity Reflection Data

HYP_BEGIN_CLASS(Entity, 166, 12, NAME("Node"))
    HypMethod(NAME(HYP_STR(SerializeComponents)), &Entity::SerializeComponents, Span<const HypClassAttribute> { {HypClassAttribute("property", "Components"), HypClassAttribute("noscriptbindings", true) } }),
    HypMethod(NAME(HYP_STR(DeserializeComponents)), &Entity::DeserializeComponents, Span<const HypClassAttribute> { {HypClassAttribute("property", "Components"), HypClassAttribute("noscriptbindings", true), HypClassAttribute("loadorder", 100) } })
HYP_END_CLASS

#pragma endregion Entity Reflection Data

} // namespace hyperion

