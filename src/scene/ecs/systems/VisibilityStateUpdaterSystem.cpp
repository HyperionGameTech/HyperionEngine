#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Octree.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void VisibilityStateUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Octree &octree = g_engine->GetWorld()->GetOctree();

    const UInt8 visibility_cursor = octree.LoadVisibilityCursor();

    for (auto [entity_id, visibility_state_component] : entity_manager.GetEntitySet<VisibilityStateComponent>()) {
        Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id);
        DebugLog(LogType::Debug, "Got octant %u\t%u\n", visibility_state_component.octant_id, octant ? octant->GetOctantID().GetIndex() : ~0u);

        if (octant) {

            const VisibilityState &octant_visibility_state = octant->GetVisibilityState();

            visibility_state_component.visibility_state.snapshots[visibility_cursor] = octant_visibility_state.snapshots[visibility_cursor];
        }
    }
}

} // namespace hyperion::v2