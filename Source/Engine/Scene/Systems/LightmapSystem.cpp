/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/LightmapSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <LightmapSystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Lightmap);

LightmapSystem::LightmapSystem()
    : m_nextLightmapVolumeId(0)
{
}

HYP_NODISCARD LightmapVolumeId LightmapSystem::AllocateLightmapVolumeId()
{
    if (World* world = GetWorld())
    {
        world->MarkDirty();
    }

    uint32 nextIdValue;

    do
    {
        // We don't want to trample over IDs that were used for LightmapVolumes that were removed from the scene.
        nextIdValue = m_nextLightmapVolumeId++;
    }
    while (m_freedLightmapVolumeIds.Contains(nextIdValue));

    return static_cast<LightmapVolumeId>(nextIdValue);
}

void LightmapSystem::OnAddedToWorld(World* world)
{
    // Set LightmapVolumes to a valid ID if they don't have one assigned already.

    for (Scene* scene : world->GetScenes())
    {
        EntityManager* mgr = scene->GetEntityManager();

        if (mgr != nullptr)
        {
            for (auto [lmvEntity, _] : mgr->GetEntitySet<EntityType<LightmapVolume>>())
            {
                LightmapVolume* lmv = StaticCast<LightmapVolume>(lmvEntity);

                if (lmv->GetLightmapVolumeId() == InvalidLightmapVolumeId)
                {
                    lmv->SetLightmapVolumeId(AllocateLightmapVolumeId());
                }
            }
        }
    }
}

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetComponent<LightmapElementComponent>();
    BoundingBoxComponent& boundingBoxComponent = entity->GetComponent<BoundingBoxComponent>();

    Scene* scene = entity->GetScene();
    Assert(scene != nullptr);

    // Assign to LightmapVolume if it has a valid path to a LightmapVolume but isn't assigned to one yet
    if (!lightmapElementComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(*scene, *entity, lightmapElementComponent, boundingBoxComponent))
        {
            HYP_LOG(Lightmap, Warning, "LightmapElementComponent for Entity {} could not be associated at runtime",
                    entity->GetName());
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    LightmapElementComponent* lightmapElementComponent = entity->TryGetComponent<LightmapElementComponent>();

    if (lightmapElementComponent != nullptr)
    {
        if (lightmapElementComponent->lightmapVolume.IsValid())
        {
            lightmapElementComponent->lightmapVolume.Reset();
        }

        entity->SetNeedsRenderProxyUpdate();
    }
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

bool LightmapSystem::AssignLightmapVolume(
    Scene& scene,
    Entity& srcEntity,
    LightmapElementComponent& lightmapElementComponent,
    BoundingBoxComponent& boundingBoxComponent)
{
    for (auto [lmvEntity, _] : scene.GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        LightmapVolume* lightmapVolume = StaticCast<LightmapVolume>(lmvEntity);

        if (lightmapElementComponent.lightmapVolume.GetUnsafe() != lightmapVolume
            && lightmapElementComponent.GetTopAssignment() == lightmapVolume->GetLightmapVolumeId())
        {
            const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                return false;
            }

            lightmapElementComponent.lightmapVolume = MakeWeakRef(lightmapVolume);

            srcEntity.SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    return false;
}

} // namespace Hyperion
