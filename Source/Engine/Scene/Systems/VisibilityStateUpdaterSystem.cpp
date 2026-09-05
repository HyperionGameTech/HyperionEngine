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

} // namespace Hyperion
