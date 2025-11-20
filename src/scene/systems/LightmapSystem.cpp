/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/LightmapSystem.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/Entity.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <core/logging/Logger.hpp>

#include <LightmapSystem.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);

    if (meshComponent.lightmapVolumeUuid == Uuid::Invalid())
    {
        meshComponent.lightmapVolume.Reset();

        entity->GetEntityManager()->RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

        return;
    }

    entity->GetEntityManager()->AddTag<EntityTag::LIGHTMAP_ELEMENT>(entity);

    if (!meshComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(entity->GetScene(), meshComponent))
        {
            HYP_LOG(Lightmap, Warning, "MeshComponent has volume Uuid: {} could not be assigned to a LightmapVolume",
                meshComponent.lightmapVolumeUuid);

            return;
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);
    meshComponent.lightmapVolume.Reset();

    entity->GetEntityManager()->RemoveTag<EntityTag::LIGHTMAP_ELEMENT>(entity);
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::LIGHTMAP_ELEMENT>>().GetScopedView(GetComponentInfos()))
        {
            if (meshComponent.lightmapVolumeUuid == Uuid::Invalid())
            {
                continue;
            }

            if (!meshComponent.lightmapVolume.IsValid())
            {
                if (!AssignLightmapVolume(scene, meshComponent))
                {
                    HYP_LOG(Lightmap, Warning, "MeshComponent has volume uuid: {} could not be assigned to a LightmapVolume",
                        meshComponent.lightmapVolumeUuid);
                }
            }
        }
    }
}

bool LightmapSystem::AssignLightmapVolume(Scene* scene, MeshComponent& meshComponent)
{
    for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
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
