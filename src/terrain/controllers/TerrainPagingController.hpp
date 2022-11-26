#ifndef HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H
#define HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H

#include "../TerrainMeshBuilder.hpp"
#include <TaskSystem.hpp>
#include <core/OpaqueHandle.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <scene/controllers/PagingController.hpp>
#include <rendering/Material.hpp>
#include <util/NoiseFactory.hpp>

#include <atomic>

namespace hyperion::v2 {

class TerrainPagingController : public PagingController
{
public:
    TerrainPagingController(
        Seed seed,
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~TerrainPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

    void AddEnqueuedChunks();

    NoiseCombinator m_noise_combinator;

private:
    struct TerrainGenerationResult
    {
        PatchInfo patch_info;
        Handle<Mesh> mesh;
    };

    Seed m_seed;

    UInt m_update_log_timer = 0;

    Handle<Material> m_material;

    BinarySemaphore m_terrain_generation_sp;
    std::atomic_bool m_terrain_generation_flag;

    // don't touch without using the semaphore
    Queue<TerrainGenerationResult> m_shared_terrain_mesh_queue;
    Queue<TerrainGenerationResult> m_owned_terrain_mesh_queue;

    FlatMap<PatchCoord, TaskRef> m_enqueued_patches;
};

} // namespace hyperion::v2

#endif