#ifndef HYPERION_V2_ECS_TERRAIN_SYSTEM_HPP
#define HYPERION_V2_ECS_TERRAIN_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <core/lib/HashMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>

#include <TaskSystem.hpp>

#include <util/NoiseFactory.hpp>

namespace hyperion::v2 {

class TerrainSystem : public System<
    ComponentDescriptor<TerrainComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~TerrainSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;

private:
    struct TerrainGenerationResult
    {
        TerrainPatchInfo    patch_info;
        Handle<Mesh>        mesh;
    };

    struct TerrainPatchUpdate
    {
        TerrainPatchCoord   coord;
        TerrainPatchState   state;
    };

    struct TerrainGenerationQueue
    {
        Mutex                           mutex;
        Queue<TerrainGenerationResult>  queue;
        AtomicVar<bool>                 has_updates;
    };

    struct TerrainGenerationState
    {
        FlatMap<TerrainPatchCoord, TaskRef>     patch_generation_tasks;
        Queue<TerrainGenerationResult>          patch_generation_queue_owned;
        RC<TerrainGenerationQueue>              patch_generation_queue_shared;

        FlatMap<TerrainPatchCoord, ID<Entity>>  patch_entities;
        FlatSet<TerrainPatchCoord>              queued_neighbors;

        Queue<TerrainPatchUpdate>               patch_update_queue;

        RC<NoiseCombinator>                     noise_combinator;

        ID<Entity> GetPatchEntity(const TerrainPatchCoord &coord) const
        {
            const auto it = patch_entities.Find(coord);

            if (it != patch_entities.End()) {
                return it->second;
            }

            return {};
        }
    };

    HashMap<ID<Entity>, RC<TerrainGenerationState>> m_states;
};

} // namespace hyperion::v2

#endif