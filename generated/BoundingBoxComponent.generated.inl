#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region BoundingBoxComponent Reflection Data

HYP_BEGIN_STRUCT(BoundingBoxComponent, 377, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 64),ClassAttribute("editor", false))
    Field(NAME(HYP_STR(LocalAabb)), &BoundingBoxComponent::localAabb, offsetof(BoundingBoxComponent, localAabb), Span<const ClassAttribute> { {ClassAttribute("property", "LocalAABB") } }),
    Field(NAME(HYP_STR(WorldAabb)), &BoundingBoxComponent::worldAabb, offsetof(BoundingBoxComponent, worldAabb), Span<const ClassAttribute> { {ClassAttribute("property", "WorldAABB") } })
HYP_END_STRUCT

#pragma endregion BoundingBoxComponent Reflection Data

HYP_REGISTER_COMPONENT(BoundingBoxComponent);
static_assert(sizeof(BoundingBoxComponent) == 64, "Expected sizeof(BoundingBoxComponent) to be 64 bytes");
} // namespace hyperion

