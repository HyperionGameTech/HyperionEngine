/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/InstanceGroupSystem.hpp>
#include <Scene/Instancing/InstanceGroup.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Core/Threading/Threads.hpp>

#include <InstanceGroupSystem.generated.inl>

namespace Hyperion {

bool InstanceGroupSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void InstanceGroupSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    m_delegateHandlers.Add(
        NAME("OnPrefabChanged"),
        Prefab::OnPrefabChanged.BindThreaded(
            [this](Prefab* prefab)
            {
                OnPrefabChanged(prefab);
            },
            g_simThread));
}

void InstanceGroupSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    InstanceGroup* group = static_cast<InstanceGroup*>(entity);

    group->MarkAllCellsDirty();
}

void InstanceGroupSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    InstanceGroup* group = static_cast<InstanceGroup*>(entity);

    group->ReleaseClusters(m_releasedEntities);
    group->m_needsFullRebuild = true;
}

void InstanceGroupSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [group, _] : scene->GetEntityManager()->GetEntitySet<EntityType<InstanceGroup>, TagComponent<EntityTag::UpdateInstanceGroup>>().GetScopedView(GetComponentInfos()))
        {
            m_groupsToUpdate.PushBack(group->WeakHandleFromThis());
        }
    }

    if (m_groupsToUpdate.Empty() && m_releasedEntities.Empty())
    {
        return;
    }

    AfterProcess(
        [this]()
        {
            Array<WeakHandle<InstanceGroup>, SceneAllocator> groupsToUpdate = std::move(m_groupsToUpdate);

            for (const WeakHandle<InstanceGroup>& weakGroup : groupsToUpdate)
            {
                Handle<InstanceGroup> group = weakGroup.Lock();

                if (!group.IsValid()
                    || group->GetEntityManager() == nullptr
                    || group->GetScene() == nullptr
                    || !ShouldProcessScene(group->GetScene()))
                {
                    continue;
                }

                const uint32 revision = group->m_revision;

                const bool isMoving = group->UpdateClusters(m_releasedEntities);

                // an edit made during the update, or a move still settling its previous transforms, keeps it for next time
                if (!isMoving && group->m_revision == revision && !group->m_needsFullRebuild)
                {
                    group->RemoveTag<EntityTag::UpdateInstanceGroup>();
                }
            }

            RemoveReleasedEntities();
        });
}

void InstanceGroupSystem::RemoveReleasedEntities()
{
    Array<Handle<Entity>, SceneAllocator> releasedEntities = std::move(m_releasedEntities);

    for (const Handle<Entity>& entity : releasedEntities)
    {
        // an entity manager that already shut down has let go of it
        if (entity.IsValid() && entity->GetEntityManager() != nullptr)
        {
            entity->Remove(/* moveToDetached */ false);
        }
    }
}

void InstanceGroupSystem::OnPrefabChanged(Prefab* prefab)
{
    World* world = GetWorld();

    if (!world || !prefab)
    {
        return;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !ShouldProcessScene(scene.Get()))
        {
            continue;
        }

        // marking adds a tag, which can't happen while the entity set is being walked
        Array<InstanceGroup*, SceneAllocator> affectedGroups;

        for (auto [group] : scene->GetEntityManager()->GetEntitySet<EntityType<InstanceGroup>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (group->GetPrefab().Get() == prefab)
            {
                affectedGroups.PushBack(group);
            }
        }

        for (InstanceGroup* group : affectedGroups)
        {
            group->MarkAllCellsDirty();
        }
    }
}

} // namespace Hyperion
