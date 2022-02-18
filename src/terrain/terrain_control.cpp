#include "terrain_control.h"
#include "../rendering/renderers/bounding_box_renderer.h"
#include "populators/grass_populator.h"
#include "populators/tree_populator.h"
#include "../util.h"
#include "../asset/asset_manager.h"

#include <thread>

namespace hyperion {

static int num_threads = 0;

TerrainControl::TerrainControl(const fbom::FBOMType &loadable_type, Camera *camera)
    : EntityControl(fbom::FBOMObjectType("TERRAIN_CONTROL").Extend(loadable_type), 30.0),
      m_camera(camera),
      m_scale(8, 4, 8),
      m_tick(0),
      m_queuetick(0),
      m_max_distance(9.0)
{
}

TerrainControl::~TerrainControl()
{
}

void TerrainControl::OnAdded()
{
    m_chunks.reserve(12);
    AddChunk(0, 0);
}

void TerrainControl::OnRemoved()
{
}

void TerrainControl::OnUpdate(double dt)
{
    if (m_camera == nullptr) {
        return;
    }

    Vector3 campos(m_camera->GetTranslation());
    campos -= parent->GetGlobalTransform().GetTranslation();
    campos *= Vector3::One() / (m_scale * float(m_chunk_size - 1));
    Vector2 v2cam(campos.x, campos.z);

    if (m_queuetick >= TERRAIN_MAX_QUEUE_TICK) {
        while (!m_queue.empty()) {
            NeighborChunkInfo *info = m_queue.front();
            AddChunk((int)info->m_position.x, (int)info->m_position.y);
            info->m_in_queue = false;
            m_queue.pop();
        }

        m_queuetick = 0;
    }

    m_queuetick += TERRAIN_UPDATE_STEP;

    if (m_tick >= TERRAIN_MAX_UPDATE_TICK) {

        for (size_t chunk_index = 0; chunk_index < m_chunks.size(); chunk_index++) {
            auto &chunk = m_chunks[chunk_index];

            if (chunk == nullptr) {
                continue;
            }

            switch (chunk->m_chunk_info.m_page_state) {
                case PageState::WAITING:
                    chunk->m_chunk_info.m_page_state = PageState::LOADED;
                    chunk->OnAdded();

                    chunk->SetLocalTranslation(Vector3(
                        (chunk->m_chunk_info.m_position.x - 0.5f) * (m_chunk_size - 1) * m_scale.x,
                        0,
                        (chunk->m_chunk_info.m_position.y - 0.5f) * (m_chunk_size - 1) * m_scale.z
                    ));

                    chunk->AddControl(std::make_shared<GrassPopulator>(m_camera));
                    chunk->AddControl(std::make_shared<TreePopulator>(m_camera));

                    // fallthrough
                case PageState::LOADED:
                    if (chunk->m_chunk_info.Center().Distance(v2cam) >= m_max_distance) {
                        chunk->m_chunk_info.m_page_state = PageState::UNLOADING;
                    } else {
                        if (chunk->GetParent() == nullptr) {
                            parent->AddChild(chunk);
                        }

                        for (auto &it : chunk->m_chunk_info.m_neighboring_chunks) {
                            if (!it.m_in_queue) {
                                if (it.Center().Distance(v2cam) < m_max_distance) {
                                    it.m_in_queue = true;
                                    m_queue.push(&it);
                                }
                            }
                        }
                    }

                    break;
                case PageState::UNLOADING:
                    chunk->m_chunk_info.m_unload_time += TERRAIN_UPDATE_STEP;

                    if (chunk->m_chunk_info.m_unload_time >= TERRAIN_MAX_UNLOAD_TICK) {
                        chunk->m_chunk_info.m_page_state = PageState::UNLOADED;
                    }

                    break;
                case PageState::UNLOADED:
                    if (chunk->GetParent() == parent) {
                        parent->RemoveChild(chunk);
                    }

                    m_chunks.erase(m_chunks.begin() + chunk_index);

                    break;
            }
        }

        m_tick = 0;
    }

    m_tick += TERRAIN_UPDATE_STEP;
}

void TerrainControl::AddChunk(int x, int z)
{
    std::shared_ptr<TerrainChunk> chunk = GetChunk(x, z);

    if (chunk == nullptr) {
        auto lambda = [this, x, z]() {
#if TERRAIN_MULTITHREADED
            num_threads++;
#endif

            ChunkInfo height_info(Vector2(x, z), m_scale);
            height_info.m_length = m_chunk_size;
            height_info.m_width = m_chunk_size;
            height_info.m_page_state = PageState::WAITING;
            height_info.m_neighboring_chunks = GetNeighbors(x, z);

            std::shared_ptr<TerrainChunk> new_chunk = NewChunk(height_info);

            AssertThrow(new_chunk != nullptr);

#if TERRAIN_MULTITHREADED
            terrain_mtx.lock();
            m_chunks.push_back(new_chunk);
            terrain_mtx.unlock();

            num_threads--;
#else
            m_chunks.push_back(new_chunk);
#endif
        };

#if TERRAIN_MULTITHREADED
        // start thread to create terrain chunk
        std::thread th(lambda);
        th.detach();
#else
        lambda();
#endif
    }
}

std::shared_ptr<TerrainChunk> TerrainControl::GetChunk(int x, int z)
{
    for (auto &chunk : m_chunks) {
        if (chunk != nullptr) {
            if ((int)chunk->m_chunk_info.m_position.x == x &&
                (int)chunk->m_chunk_info.m_position.y == z) {

                return chunk;
            }
        }
    }

    return nullptr;
}

std::array<NeighborChunkInfo, 8> TerrainControl::GetNeighbors(int x, int z)
{
    std::array<NeighborChunkInfo, 8> neighbors = {
        Vector2(x + 1, z),
        Vector2(x - 1, z),
        Vector2(x, z + 1),
        Vector2(x, z - 1),
        Vector2(x + 1, z - 1),
        Vector2(x - 1, z - 1),
        Vector2(x + 1, z + 1),
        Vector2(x - 1, z + 1),
    };
    return neighbors;
}

} // namespace hyperion
