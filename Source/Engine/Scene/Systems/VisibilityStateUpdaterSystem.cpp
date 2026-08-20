/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/VisibilityStateUpdaterSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/SceneOctree.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <VisibilityStateUpdaterSystem.generated.inl>

namespace Hyperion {

bool VisibilityStateUpdaterSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::HAS_OCTREE;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void VisibilityStateUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    EntityManager& entityManager = *entity->GetEntityManager();

    // entity->GetScene() already reports the destination scene mid-move, so gate on the EntityManager's scene instead.
    if (!ShouldProcessScene(entityManager.GetScene()))
    {
        return;
    }

    VisibilityStateComponent* visibilityStateComponent = entityManager.TryGetComponent<VisibilityStateComponent>(entity);

    if (!visibilityStateComponent)
    {
        return;
    }

    SceneOctree& octree = entityManager.GetScene()->GetOctree();

    const SceneOctree::Result removeResult = octree.Remove(entity);

    if (removeResult.HasError())
    {
        HYP_LOG(Scene, Warning, "Failed to remove Entity {} from octree: {}", entity->GetName(), removeResult.GetError().GetMessage());
    }

    visibilityStateComponent->octantId = OctantId::Invalid();
    visibilityStateComponent->visibilityState = nullptr;
}

// This has been replaced by VisThread
// @TODO Remove
#if 0

void VisibilityStateUpdaterSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        SceneOctree& octree = scene->GetOctree();

        Array<WeakHandle<Entity>, SceneAllocator> updatedEntities;

        const auto updateVisbilityState = [&octree, &updatedEntities](Entity* entity, VisibilityStateComponent& visibilityStateComponent, BoundingBoxComponent& boundingBoxComponent)
        {
            const bool visibilityStateInvalidated = visibilityStateComponent.flags & VisibilityStateFlags::INVALIDATED;

            visibilityStateComponent.flags &= ~VisibilityStateFlags::INVALIDATED;

            if (!boundingBoxComponent.worldAabb.IsValid() || !boundingBoxComponent.worldAabb.IsFinite() || (entity->GetNodeFlags() & NodeFlags::ExcludeFromOctree))
            {
                visibilityStateComponent.octantId = OctantId::Invalid();
                visibilityStateComponent.visibilityState = nullptr;

                return;
            }

            // if entity is not in the octree, try to insert it
            if (visibilityStateComponent.octantId == OctantId::Invalid())
            {
                visibilityStateComponent.visibilityState = nullptr;

                const SceneOctree::Result insertResult = octree.Insert(entity, boundingBoxComponent.worldAabb);

                if (insertResult.HasValue())
                {
                    AssertDebug(insertResult.GetValue() != OctantId::Invalid(), "Invalid OctantId returned from Insert()");

                    visibilityStateComponent.octantId = insertResult.GetValue();

                    if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
                    {
                        visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
                    }

                    updatedEntities.PushBack(MakeWeakRef(entity));
                }

                return;
            }

            visibilityStateComponent.visibilityState = nullptr;

            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the entire octree to be updated.
            const bool forceEntryInvalidation = visibilityStateInvalidated;

            const SceneOctree::Result updateResult = octree.Update(entity, boundingBoxComponent.worldAabb, forceEntryInvalidation);

            if (updateResult.HasError())
            {
                visibilityStateComponent.octantId = OctantId::Invalid();

                HYP_LOG(Scene, Warning, "Failed to update entity {} in octree: {}", entity->Id(), updateResult.GetError().GetMessage());

                return;
            }

            visibilityStateComponent.octantId = updateResult.GetValue();

            if (visibilityStateComponent.octantId.IsInvalid())
            {
                AssertDebug(false, "Invalid OctantId returned from Update()");

                return;
            }

            visibilityStateComponent.visibilityState = nullptr;

            if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
            {
                visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
            }

            updatedEntities.PushBack(MakeWeakRef(entity));
        };

        for (auto [entity, visibilityStateComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<VisibilityStateComponent, BoundingBoxComponent, TagComponent<EntityTag::UpdateVisibility>>().GetScopedView(GetComponentInfos()))
        {
            updateVisbilityState(entity, visibilityStateComponent, boundingBoxComponent);
        }

        if (updatedEntities.Any())
        {
#if HYP_DEBUG_MODE
            if (updatedEntities.Size() >= 128)
            {
                HYP_LOG(Scene, Warning, "Updating visibility states for a lot of entities ({})! This will have a performance impact if it happens frequently."
                                        "\n\tMaybe the Scene's octree should have a different bounding size or be broken into multiple Scenes."
                                        "\n\tScene name: {}, flags: {}",
                        updatedEntities.Size(), scene->GetName(), uint32(scene->GetSceneFlags()));
            }
#endif

            AfterProcess(
                [scene, updatedEntities = std::move(updatedEntities)]()
                {
                    for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                    {
                        scene->GetEntityManager()->RemoveTag<EntityTag::UpdateVisibility>(entityWeak.GetUnsafe());
                    }
                });
        }
    }
}

#endif

} // namespace Hyperion
