#ifndef TERRAIN_CONTROL_H
#define TERRAIN_CONTROL_H

#include "../control.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "terrain_chunk.h"

#include <queue>
#include <memory>

#define TERRAIN_CHUNK_VERTEX_SIZE 16
#define TERRAIN_MAX_QUEUE_TICK 0.2
#define TERRAIN_MAX_UPDATE_TICK 0.2
#define TERRAIN_MAX_UNLOAD_TICK 1
#define TERRAIN_UPDATE_STEP 1
#define TERRAIN_MULTITHREADED 0

#if TERRAIN_MULTITHREADED
#include <mutex>
#endif

namespace hyperion {

class TerrainControl : public EntityControl {
public:
    TerrainControl(const fbom::FBOMType &loadable_type, Camera *camera);
    virtual ~TerrainControl();

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() = 0;

    Camera *m_camera;
    Vector3 m_scale;
    int m_chunk_size = TERRAIN_CHUNK_VERTEX_SIZE;
    std::queue<NeighborChunkInfo*> m_queue;
    std::vector<std::shared_ptr<TerrainChunk>> m_chunks;

    virtual std::shared_ptr<TerrainChunk> NewChunk(const ChunkInfo &chunk_info) = 0;

private:
    void AddChunk(int x, int z);
    std::shared_ptr<TerrainChunk> GetChunk(int x, int z);
    std::array<NeighborChunkInfo, 8> GetNeighbors(int x, int z);

    int m_tick;
    int m_queuetick;
    int m_chunk_index = 0;
    double m_max_distance;

#if TERRAIN_MULTITHREADED
    std::mutex terrain_mtx;
#endif
};

} // namespace hyperion

#endif
