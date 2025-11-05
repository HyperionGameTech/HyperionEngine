#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region LightmapVolumeComponent Reflection Data

HYP_BEGIN_STRUCT(LightmapVolumeComponent, 382, 0, {}, ClassAttribute("component", true))
    Field(NAME(HYP_STR(Volume)), &LightmapVolumeComponent::volume, offsetof(LightmapVolumeComponent, volume), Span<const ClassAttribute> { {ClassAttribute("property", "Volume") } })
HYP_END_STRUCT

#pragma endregion LightmapVolumeComponent Reflection Data

HYP_REGISTER_COMPONENT(LightmapVolumeComponent);
} // namespace hyperion

