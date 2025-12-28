/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/systems/LightmapSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/Entity.hpp>
#include <scene/LightmapVolume.hpp>

#include <LightmapSystem.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetEntityManager()->GetComponent<LightmapElementComponent>(entity);

    if (!lightmapElementComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(entity->GetScene(), lightmapElementComponent))
        {
            HYP_LOG(Lightmap, Warning, "LightmapElementComponent of {} has volume UUID: {} could not be assigned to a LightmapVolume",
                entity->GetName(),
                lightmapElementComponent.lightmapVolumeUuid);

            return;
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

bool LightmapSystem::AssignLightmapVolume(Scene* scene, LightmapElementComponent& lightmapElementComponent)
{
    for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        LightmapVolume* lightmapVolume = ObjCast<LightmapVolume>(entity);
        Assert(lightmapVolume != nullptr);

        if (lightmapVolume->GetUUID() == lightmapElementComponent.lightmapVolumeUuid
            && lightmapElementComponent.lightmapVolume.GetUnsafe() != lightmapVolume)
        {
            const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                HYP_BREAKPOINT;
                return false;
            }

            lightmapElementComponent.lightmapVolume = MakeWeakRef(lightmapVolume);

            entity->SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    return false;
}

} // namespace Hyperion
