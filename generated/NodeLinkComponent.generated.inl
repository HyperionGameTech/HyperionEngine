#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region NodeLinkComponent Reflection Data

HYP_BEGIN_STRUCT(NodeLinkComponent, 380, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 8),ClassAttribute("serialize", false),ClassAttribute("editor", false))
    Field(NAME(HYP_STR(Node)), &NodeLinkComponent::node, offsetof(NodeLinkComponent, node))
HYP_END_STRUCT

#pragma endregion NodeLinkComponent Reflection Data

HYP_REGISTER_COMPONENT(NodeLinkComponent);
static_assert(sizeof(NodeLinkComponent) == 8, "Expected sizeof(NodeLinkComponent) to be 8 bytes");
} // namespace hyperion

