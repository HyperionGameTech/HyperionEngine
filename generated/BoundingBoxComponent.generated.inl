#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region BoundingBoxComponent Reflection Data

HYP_BEGIN_STRUCT(BoundingBoxComponent, 394, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 64),HypClassAttribute("editor", false))
    HypField(NAME(HYP_STR(LocalAabb)), &BoundingBoxComponent::localAabb, offsetof(BoundingBoxComponent, localAabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "LocalAABB") } }),
    HypField(NAME(HYP_STR(WorldAabb)), &BoundingBoxComponent::worldAabb, offsetof(BoundingBoxComponent, worldAabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "WorldAABB") } })
HYP_END_STRUCT

#pragma endregion BoundingBoxComponent Reflection Data

HYP_REGISTER_COMPONENT(BoundingBoxComponent);
static_assert(sizeof(BoundingBoxComponent) == 64, "Expected sizeof(BoundingBoxComponent) to be 64 bytes");
} // namespace hyperion

