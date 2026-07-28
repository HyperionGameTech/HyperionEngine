#include <ScenePch.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>

#include <Scene/LightmapVolume.hpp>

#include <LightmapElementComponent.generated.inl>

namespace Hyperion {

LightmapElementComponent::LightmapElementComponent()
    : lightmapElementId(Invalid<LightmapElementId>),
      lightmapVolumeAssignmentWeights {}
{
    std::fill(
        lightmapVolumeAssignments.Begin(),
        lightmapVolumeAssignments.End(),
        InvalidLightmapVolumeId);
}

} // namespace Hyperion
