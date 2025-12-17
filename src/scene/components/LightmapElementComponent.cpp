#include <ScenePch.hpp>

#include <scene/components/LightmapElementComponent.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <LightmapElementComponent.generated.inl>

namespace hyperion {

LightmapElementComponent::LightmapElementComponent()
    : lightmapVolumeUuid(Uuid::Invalid()),
      lightmapElementId(InvalidLightmapElementId)
{
}

} // namespace hyperion
