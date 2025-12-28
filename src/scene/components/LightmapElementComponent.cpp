#include <ScenePch.hpp>

#include <scene/components/LightmapElementComponent.hpp>

#include <scene/LightmapVolume.hpp>

#include <LightmapElementComponent.generated.inl>

namespace Hyperion {

LightmapElementComponent::LightmapElementComponent()
    : lightmapVolumeUuid(Uuid::Invalid()),
      lightmapElementId(InvalidLightmapElementId)
{
}

} // namespace Hyperion
