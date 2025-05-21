/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_HPP
#define HYPERION_WORLD_GRID_HPP

#include <core/Handle.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <core/math/Vector2.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Scene;
class EntityManager;
class WorldGridPlugin;

HYP_ENUM()
enum class WorldGridPatchState
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

HYP_STRUCT()
struct WorldGridPatchNeighbor
{
    HYP_FIELD(Serialize, Property="Coord")
    Vec2i   coord;

    HYP_FORCE_INLINE Vec2f GetCenter() const
        { return Vec2f(coord) - 0.5f; }
};

struct WorldGridPatchUpdate
{
    Vec2i               coord;
    WorldGridPatchState state;
};

HYP_STRUCT()
struct WorldGridPatchInfo
{
    HYP_FIELD(Serialize, Property="Extent")
    Vec3i                                   extent;

    HYP_FIELD(Serialize, Property="Coord")
    Vec2i                                   coord;
    
    HYP_FIELD(Serialize, Property="Scale")
    Vec3f                                   scale = Vec3f::One();

    HYP_FIELD(Serialize, Property="State")
    WorldGridPatchState                     state = WorldGridPatchState::UNLOADED;

    HYP_FIELD(Serialize, Property="Neighbors")
    FixedArray<WorldGridPatchNeighbor, 8>   neighbors;

    HYP_FIELD()
    float                                   unload_timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(extent);
        hash_code.Add(coord);
        hash_code.Add(scale);
        hash_code.Add(state);

        return hash_code;
    }
};

class HYP_API WorldGridPatch
{
public:
    WorldGridPatch(const WorldGridPatchInfo &patch_info);
    virtual ~WorldGridPatch();

    HYP_FORCE_INLINE const WorldGridPatchInfo &GetPatchInfo() const
        { return m_patch_info; }

    HYP_FORCE_INLINE void SetState(WorldGridPatchState state)
        { m_patch_info.state = state; }

    virtual void InitializeEntity(const Handle<Scene> &scene, ID<Entity> entity) = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;

protected:
    WorldGridPatchInfo  m_patch_info;
};

struct WorldGridPatchDesc
{
    WorldGridPatchInfo  patch_info;

    Handle<Entity>      entity;
    
    // May be null if the patch is not yet created
    RC<WorldGridPatch>  patch;
};

struct WorldGridPatchGenerationQueue
{
    Mutex                       mutex;
    Queue<RC<WorldGridPatch>>   queue;
    AtomicVar<bool>             has_updates;
};

struct WorldGridState
{
    FlatMap<Vec2i, Task<void>>          patch_generation_tasks;
    Queue<RC<WorldGridPatch>>           patch_generation_queue_owned;
    WorldGridPatchGenerationQueue       patch_generation_queue_shared;
    
    Queue<WorldGridPatchUpdate>         patch_update_queue;
    AtomicVar<uint32>                   patch_update_queue_size { 0 };
    mutable Mutex                       patch_update_queue_mutex;

    FlatMap<Vec2i, WorldGridPatchDesc>  patches;
    mutable Mutex                       patches_mutex;

    // Keep track of the last desired patches to avoid unnecessary comparison and locking
    Array<Vec2i>                        previous_desired_patch_coords;

    void PushUpdate(WorldGridPatchUpdate &&update);

    void AddPatch(const Vec2i &coord, WorldGridPatchDesc &&patch_desc)
    {
        Mutex::Guard guard(patches_mutex);

        patches.Insert(coord, std::move(patch_desc));
    }

    bool RemovePatch(const Vec2i &coord)
    {
        Mutex::Guard guard(patches_mutex);

        const auto it = patches.Find(coord);

        if (it != patches.End()) {
            patches.Erase(it);
            return true;
        }

        return false;
    }

    WorldGridPatchDesc *GetPatchDesc(const Vec2i &coord)
    {
        Mutex::Guard guard(patches_mutex);

        const auto it = patches.Find(coord);

        if (it != patches.End()) {
            return &it->second;
        }

        return nullptr;
    }

    const WorldGridPatchDesc *GetPatchDesc(const Vec2i &coord) const
    {
        Mutex::Guard guard(patches_mutex);

        const auto it = patches.Find(coord);

        if (it != patches.End()) {
            return &it->second;
        }

        return nullptr;
    }
};

struct WorldGridParams
{
    Vec3i   patch_size { 32, 32, 32 };
    Vec3f   offset { 0.0f, 0.0f, 0.0f };
    Vec3f   scale { 1.0f, 1.0f, 1.0f };
    float   max_distance = 1.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(patch_size);
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(max_distance);
        return hc;
    }
};

class HYP_API WorldGrid
{
public:
    WorldGrid();
    WorldGrid(const WorldGridParams &params, const Handle<Scene> &scene);
    WorldGrid(const WorldGrid &other)               = delete;
    WorldGrid &operator=(const WorldGrid &other)    = delete;
    WorldGrid(WorldGrid &&other)                    = delete;
    WorldGrid &operator=(WorldGrid &&other)         = delete;
    ~WorldGrid();

    HYP_FORCE_INLINE const WorldGridParams &GetParams() const
        { return m_params; }

    HYP_FORCE_INLINE const WeakHandle<Scene> &GetScene() const
        { return m_scene; }

    HYP_FORCE_INLINE const WorldGridState &GetState() const
        { return m_state; }

    void AddPlugin(int priority, RC<WorldGridPlugin> &&plugin);
    RC<WorldGridPlugin> GetPlugin(int priority) const;

    void Initialize();
    void Shutdown();
    void Update(GameCounter::TickUnit delta);

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_params);
        return hc;
    }

private:
    void GetDesiredPatches(Vec2i coord, Array<Vec2i> &out_patch_coords) const;

    RC<WorldGridPlugin> GetMainPlugin() const;

    const WorldGridParams               m_params;
    WeakHandle<Scene>                   m_scene;

    WorldGridState                      m_state;

    FlatMap<int, RC<WorldGridPlugin>>   m_plugins;

    Handle<Node>                           m_root_node;

    bool                                m_is_initialized;
};

} // namespace hyperion

#endif
