#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region LightmapVolumeComponent Reflection Data

HYP_BEGIN_STRUCT(LightmapVolumeComponent, 409, 0, {}, HypClassAttribute("component", true))
    HypField(NAME(HYP_STR(Volume)), &LightmapVolumeComponent::volume, offsetof(LightmapVolumeComponent, volume), Span<const HypClassAttribute> { {HypClassAttribute("property", "Volume") } })
HYP_END_STRUCT

#pragma endregion LightmapVolumeComponent Reflection Data

HYP_REGISTER_COMPONENT(LightmapVolumeComponent);
} // namespace hyperion

