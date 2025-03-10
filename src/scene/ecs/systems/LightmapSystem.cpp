/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/LightmapSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/containers/Array.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent &lightmap_element_component = GetEntityManager().GetComponent<LightmapElementComponent>(entity);

    if (lightmap_element_component.volume_uuid == UUID::Invalid()) {
        lightmap_element_component.volume.Reset();

        return;
    }

    if (!lightmap_element_component.volume.IsValid()) {
        if (!AssignVolumeToLightmapElement(lightmap_element_component)) {
            HYP_LOG(Lightmap, Warning, "LightmapElementComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                lightmap_element_component.volume_uuid);
        }
    }
}

void LightmapSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    LightmapElementComponent &lightmap_element_component = GetEntityManager().GetComponent<LightmapElementComponent>(entity);
    lightmap_element_component.volume.Reset();
}

void LightmapSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, lightmap_element_component] : GetEntityManager().GetEntitySet<LightmapElementComponent>().GetScopedView(GetComponentInfos())) {
        if (lightmap_element_component.volume_uuid == UUID::Invalid()) {
            continue;
        }

        if (!lightmap_element_component.volume.IsValid()) {
            if (!AssignVolumeToLightmapElement(lightmap_element_component)) {
                HYP_LOG(Lightmap, Warning, "LightmapElementComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                    lightmap_element_component.volume_uuid);
            }
        }
    }
}

bool LightmapSystem::AssignVolumeToLightmapElement(LightmapElementComponent &lightmap_element_component)
{
    for (auto [entity_id, lightmap_volume_component] : GetEntityManager().GetEntitySet<LightmapVolumeComponent>().GetScopedView(GetComponentInfos())) {
        if (!lightmap_volume_component.volume.IsValid()) {
            continue;
        }

        if (lightmap_volume_component.volume->GetUUID() == lightmap_element_component.volume_uuid) {
            lightmap_element_component.volume = lightmap_volume_component.volume.ToWeak();

            const LightmapElement *lightmap_element = lightmap_volume_component.volume->GetElement(lightmap_element_component.element_index);

            if (!lightmap_element) {
                return false;
            }

            return true;
        }
    }

    return false;
}

} // namespace hyperion
