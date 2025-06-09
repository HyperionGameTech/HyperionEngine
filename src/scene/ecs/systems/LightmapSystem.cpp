/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/LightmapSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/containers/Array.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (mesh_component.lightmap_volume_uuid == UUID::Invalid())
    {
        mesh_component.lightmap_volume.Reset();

        GetEntityManager().RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

        return;
    }

    GetEntityManager().AddTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

    if (!mesh_component.lightmap_volume.IsValid())
    {
        if (!AssignLightmapVolume(mesh_component))
        {
            HYP_LOG(Lightmap, Warning, "MeshComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                mesh_component.lightmap_volume_uuid);

            return;
        }
    }
}

void LightmapSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    mesh_component.lightmap_volume.Reset();

    GetEntityManager().RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);
}

void LightmapSystem::Process(float delta)
{
    for (auto [entity_id, mesh_component, _] : GetEntityManager().GetEntitySet<MeshComponent, EntityTagComponent<EntityTag::LIGHTMAP_ELEMENT>>().GetScopedView(GetComponentInfos()))
    {
        if (mesh_component.lightmap_volume_uuid == UUID::Invalid())
        {
            continue;
        }

        if (!mesh_component.lightmap_volume.IsValid())
        {
            if (!AssignLightmapVolume(mesh_component))
            {
                HYP_LOG(Lightmap, Warning, "MeshComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                    mesh_component.lightmap_volume_uuid);
            }
        }
    }
}

bool LightmapSystem::AssignLightmapVolume(MeshComponent& mesh_component)
{
    for (auto [entity_id, lightmap_volume_component] : GetEntityManager().GetEntitySet<LightmapVolumeComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!lightmap_volume_component.volume.IsValid())
        {
            continue;
        }

        if (lightmap_volume_component.volume->GetUUID() == mesh_component.lightmap_volume_uuid)
        {
            const LightmapElement* lightmap_element = lightmap_volume_component.volume->GetElement(mesh_component.lightmap_element_index);

            if (!lightmap_element)
            {
                return false;
            }

            mesh_component.lightmap_volume = lightmap_volume_component.volume.ToWeak();

            return true;
        }
    }

    return false;
}

} // namespace hyperion
