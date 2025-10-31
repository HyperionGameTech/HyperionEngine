#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region NodeLinkComponent Reflection Data

HYP_BEGIN_STRUCT(NodeLinkComponent, 410, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 8),HypClassAttribute("serialize", false),HypClassAttribute("editor", false))
    HypField(NAME(HYP_STR(Node)), &NodeLinkComponent::node, offsetof(NodeLinkComponent, node))
HYP_END_STRUCT

#pragma endregion NodeLinkComponent Reflection Data

HYP_REGISTER_COMPONENT(NodeLinkComponent);
static_assert(sizeof(NodeLinkComponent) == 8, "Expected sizeof(NodeLinkComponent) to be 8 bytes");
} // namespace hyperion

