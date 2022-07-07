#ifndef HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H
#define HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H

#include "../TerrainMeshBuilder.hpp"
#include <scene/controllers/PagingController.hpp>
#include <rendering/Material.hpp>

#include <mutex>
#include <atomic>
#include <queue>

namespace hyperion::v2 {

class TerrainPagingController : public PagingController {
public:
    TerrainPagingController(Seed seed, Extent3D patch_size, const Vector3 &scale);
    virtual ~TerrainPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

    void AddEnqueuedChunks();

private:
    struct TerrainGenerationResult {
        PatchInfo             patch_info;
        std::unique_ptr<Mesh> mesh;
    };

    Seed                                m_seed;

    Ref<Material>                       m_material;

    std::mutex                          m_terrain_generation_mutex;
    std::atomic_bool                    m_terrain_generation_flag;

    // don't touch without mutex
    std::queue<TerrainGenerationResult> m_shared_terrain_mesh_queue;
    std::queue<TerrainGenerationResult> m_owned_terrain_mesh_queue;
};

} // namespace hyperion::v2

#endif