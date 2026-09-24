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

#include <Rendering/StencilMasks.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <algorithm>

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
            for (auto [lmv] : mgr->GetEntitySet<EntityType<LightmapVolume>>())
            {
                if (lmv->GetLightmapVolumeId() == InvalidLightmapVolumeId)
                {
                    lmv->SetLightmapVolumeId(AllocateLightmapVolumeId());
                }

                RegisterVolume(lmv);
            }
        }
    }

    if (m_volumes.Any())
    {
        ResolveVolumeAssignments();
        AssignStencilValues();
    }
}

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetComponent<LightmapElementComponent>();

    // the owning volume may not have entered the world yet; it claims its entities when it does
    if (!ResolveVolumeForEntity(*entity, lightmapElementComponent))
    {
        HYP_LOG(Lightmap, Debug, "LightmapElementComponent for Entity {} has no lightmap volume to resolve to yet",
                entity->GetName());
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

void LightmapSystem::RegisterVolume(LightmapVolume* volume)
{
    AssertDebug(volume != nullptr);

    if (!m_volumes.Contains(volume))
    {
        m_volumes.PushBack(volume);
    }
}

void LightmapSystem::UnregisterVolume(LightmapVolume* volume)
{
    auto it = m_volumes.Find(volume);

    if (it != m_volumes.End())
    {
        m_volumes.Erase(it);
    }
}

LightmapVolume* LightmapSystem::FindVolume(LightmapVolumeId id) const
{
    if (id == InvalidLightmapVolumeId)
    {
        return nullptr;
    }

    for (LightmapVolume* volume : m_volumes)
    {
        if (volume->GetLightmapVolumeId() == id)
        {
            return volume;
        }
    }

    return nullptr;
}

LightmapVolume* LightmapSystem::ResolveVolume(const LightmapElementComponent& lightmapElementComponent) const
{
    LightmapVolume* lightmapVolume = FindVolume(lightmapElementComponent.lightmapVolumeId);

    if (!lightmapVolume)
    {
        return nullptr;
    }

    const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

    if (!lightmapElement)
    {
        return nullptr;
    }

    // GetAtlasTexture reads whatever the applied layer put in the field, so a layer with no bake falls back to probe lighting.
    if (!lightmapVolume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LightmapVolume::IrradianceTexture).IsValid())
    {
        return nullptr;
    }

    return lightmapVolume;
}

bool LightmapSystem::ApplyResolvedVolume(
    Entity& srcEntity,
    LightmapElementComponent& lightmapElementComponent,
    LightmapVolume* resolvedVolume)
{
    if (lightmapElementComponent.lightmapVolume.GetUnsafe() != resolvedVolume)
    {
        if (resolvedVolume)
        {
            lightmapElementComponent.lightmapVolume = MakeWeakRef(resolvedVolume);
        }
        else
        {
            lightmapElementComponent.lightmapVolume.Reset();
        }

        srcEntity.SetNeedsRenderProxyUpdate();
    }

    return resolvedVolume != nullptr;
}

void LightmapSystem::AssignStencilValues()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    Array<LightmapVolume*> volumes = m_volumes;

    // ordered by id so the assignment is deterministic
    std::sort(volumes.Begin(), volumes.End(), [](const LightmapVolume* lhs, const LightmapVolume* rhs)
        {
            return uint32(lhs->GetLightmapVolumeId()) < uint32(rhs->GetLightmapVolumeId());
        });

    struct AssignedRange
    {
        BoundingBox bounds;
        uint32 first;
        uint32 count;
    };

    Array<AssignedRange> assignedRanges;
    Array<LightmapVolume*> changedVolumes;

    for (LightmapVolume* volume : volumes)
    {
        const uint32 numPages = MathUtil::Max(volume->NumUsedAtlases(), 1u);
        const BoundingBox bounds = volume->GetLightingBounds();

        uint32 stencilBase = 0;

        // only volumes that can shade the same pixels need distinct values
        for (uint32 candidate = 1; candidate + numPages - 1 <= uint32(LightmapStencilMask); candidate++)
        {
            bool collides = false;

            for (const AssignedRange& range : assignedRanges)
            {
                if (!bounds.IsValid() || !range.bounds.IsValid() || !bounds.Overlaps(range.bounds))
                {
                    continue;
                }

                if (candidate < range.first + range.count && range.first < candidate + numPages)
                {
                    collides = true;

                    break;
                }
            }

            if (!collides)
            {
                stencilBase = candidate;

                break;
            }
        }

        if (stencilBase == 0)
        {
            HYP_LOG(Lightmap, Warning, "LightmapVolume '{}' overlaps too many other lightmap volumes to get a stencil value; its entities fall back to probe lighting",
                volume->GetName());
        }
        else
        {
            assignedRanges.PushBack(AssignedRange { bounds, stencilBase, numPages });
        }

        if (volume->GetStencilBase() != uint8(stencilBase))
        {
            volume->SetStencilBase(uint8(stencilBase));

            changedVolumes.PushBack(volume);
        }
    }

    if (changedVolumes.Empty())
    {
        return;
    }

    // entities carry their stencil value in their render proxy
    for (Scene* scene : world->GetScenes())
    {
        EntityManager* mgr = scene->GetEntityManager();

        if (!mgr)
        {
            continue;
        }

        for (auto [entity, lightmapElementComponent] : mgr->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (changedVolumes.Contains(lightmapElementComponent.lightmapVolume.GetUnsafe()))
            {
                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

bool LightmapSystem::ResolveVolumeForEntity(Entity& srcEntity, LightmapElementComponent& lightmapElementComponent)
{
    return ApplyResolvedVolume(srcEntity, lightmapElementComponent, ResolveVolume(lightmapElementComponent));
}

void LightmapSystem::ResolveVolumeAssignments()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    for (Scene* scene : world->GetScenes())
    {
        EntityManager* mgr = scene->GetEntityManager();

        if (!mgr)
        {
            continue;
        }

        for (auto [entity, lightmapElementComponent] : mgr->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            ApplyResolvedVolume(*entity, lightmapElementComponent, ResolveVolume(lightmapElementComponent));
        }
    }
}

} // namespace Hyperion
