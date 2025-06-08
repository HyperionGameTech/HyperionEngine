/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_HPP
#define HYPERION_WORLD_GRID_HPP

#include <scene/world_grid/WorldGridState.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <core/Handle.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <core/math/Vector2.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Scene;
class EntityManager;
class WorldGridPlugin;

struct WorldGridParams
{
    Vec2u grid_size { 64, 64 };
    Vec3u cell_size { 32, 32, 32 };
    Vec3f offset { 0.0f, 0.0f, 0.0f };
    Vec3f scale { 1.0f, 1.0f, 1.0f };
    float max_distance = 1.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(grid_size);
        hc.Add(cell_size);
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(max_distance);
        return hc;
    }
};

HYP_CLASS()
class HYP_API WorldGrid : public HypObject<WorldGrid>
{
    HYP_OBJECT_BODY(WorldGrid);

public:
    WorldGrid();
    WorldGrid(World* world, const WorldGridParams& params);
    WorldGrid(const WorldGrid& other) = delete;
    WorldGrid& operator=(const WorldGrid& other) = delete;
    WorldGrid(WorldGrid&& other) = delete;
    WorldGrid& operator=(WorldGrid&& other) = delete;
    ~WorldGrid();

    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE const WorldGridParams& GetParams() const
    {
        return m_params;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<StreamingManager>& GetStreamingManager() const
    {
        return m_streaming_manager;
    }

    HYP_METHOD()
    void AddPlugin(int priority, const RC<WorldGridPlugin>& plugin);

    HYP_METHOD()
    RC<WorldGridPlugin> GetPlugin(int priority) const;

    void Init();
    void Shutdown();
    void Update(GameCounter::TickUnit delta);

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_params);
        return hc;
    }

private:
    // void CreatePatches();

    void GetDesiredPatches(HashSet<Vec2i>& out_patch_coords) const;

    RC<WorldGridPlugin> GetMainPlugin() const;

    World* m_world;

    const WorldGridParams m_params;

    Handle<StreamingManager> m_streaming_manager;

    // Array<WorldGridPatchDesc> m_patches;

    WorldGridState m_state;

    SortedArray<KeyValuePair<int, RC<WorldGridPlugin>>> m_plugins;
};

} // namespace hyperion

#endif
