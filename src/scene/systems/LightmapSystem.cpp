/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/LightmapSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/containers/Array.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (meshComponent.lightmapVolumeUuid == UUID::Invalid())
    {
        meshComponent.lightmapVolume.Reset();

        GetEntityManager().RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

        return;
    }

    GetEntityManager().AddTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

    if (!meshComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(meshComponent))
        {
            HYP_LOG(Lightmap, Warning, "MeshComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                meshComponent.lightmapVolumeUuid);

            return;
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);
    meshComponent.lightmapVolume.Reset();

    GetEntityManager().RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);
}

void LightmapSystem::Process(float delta)
{
    auto lightmapVolumesView = GetEntityManager().GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos());
    if (lightmapVolumesView.GetElements().Empty())
    {
        return; // no point in processing if there are no volumes in this Scene
    }
    
    for (auto [entity, meshComponent, _] : GetEntityManager().GetEntitySet<MeshComponent, EntityTagComponent<EntityTag::LIGHTMAP_ELEMENT>>().GetScopedView(GetComponentInfos()))
    {
        if (meshComponent.lightmapVolumeUuid == UUID::Invalid())
        {
            continue;
        }

        if (!meshComponent.lightmapVolume.IsValid())
        {
            if (!AssignLightmapVolume(meshComponent))
            {
                HYP_LOG(Lightmap, Warning, "MeshComponent has volume UUID: {} could not be assigned to a LightmapVolume",
                    meshComponent.lightmapVolumeUuid);
            }
        }
    }
}

bool LightmapSystem::AssignLightmapVolume(MeshComponent& meshComponent)
{
    for (auto [entity, _] : GetEntityManager().GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        LightmapVolume* lightmapVolume = ObjCast<LightmapVolume>(entity);
        Assert(lightmapVolume != nullptr);

        if (lightmapVolume->GetUUID() == meshComponent.lightmapVolumeUuid)
        {
            const LightmapElement* lightmapElement = lightmapVolume->GetElement(meshComponent.lightmapElementId);

            if (!lightmapElement)
            {
                return false;
            }

            meshComponent.lightmapVolume = MakeWeakRef(lightmapVolume);

            return true;
        }
    }

    return false;
}

} // namespace hyperion
