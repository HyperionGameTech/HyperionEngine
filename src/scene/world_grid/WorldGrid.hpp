/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/world_grid/WorldGridState.hpp>

#include <scene/Entity.hpp>

#include <core/reflection/Handle.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <core/math/Vector2.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class Scene;
class EntityManager;
class WorldGridLayer;
class StreamingManager;

HYP_CLASS()
class HYP_API WorldGrid final : public HypObjectBase
{
    HYP_OBJECT_BODY(WorldGrid);

public:
    WorldGrid();
    WorldGrid(World* world);
    WorldGrid(const WorldGrid& other) = delete;
    WorldGrid& operator=(const WorldGrid& other) = delete;
    WorldGrid(WorldGrid&& other) = delete;
    WorldGrid& operator=(WorldGrid&& other) = delete;
    ~WorldGrid() override;

    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    void AddLayer(const Handle<WorldGridLayer>& layer);

    HYP_METHOD()
    bool RemoveLayer(WorldGridLayer* layer);

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<Handle<WorldGridLayer>>& GetLayers() const
    {
        return m_layers;
    }

    void Shutdown();
    void Update(float delta);

private:
    void Init() override;

    // void CreatePatches();

    void GetDesiredPatches(HashSet<Vec2i>& outPatchCoords) const;

    World* m_world;

    // Array<WorldGridPatchDesc> m_patches;

    WorldGridState m_state;

    HYP_FIELD(Property = "Layers", Serialize = true)
    Array<Handle<WorldGridLayer>> m_layers;
};

} // namespace hyperion
