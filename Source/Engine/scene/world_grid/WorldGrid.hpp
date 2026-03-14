/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/world_grid/WorldGridState.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <scene/Entity.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/FlatMap.hpp>
#include <Core/containers/FlatSet.hpp>
#include <Core/containers/Queue.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/threading/Task.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/math/Vector2.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class Scene;
class EntityManager;
class StreamingManager;

struct WGState;

HYP_STRUCT()
struct WGObject
{
    HYP_STRUCT_BODY(WGObject);

    HYP_FIELD()
    Vec2i coords;

    HYP_FIELD(FollowAssetPath = true)
    AssetPath path;

    HYP_FORCE_INLINE bool operator==(const WGObject& other) const
    {
        return coords == other.coords
            && path == other.path;
    }

    HYP_FORCE_INLINE bool operator!=(const WGObject& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(coords)
            .Combine(HashCode::GetHashCode(path));
    }
};

HYP_STRUCT()
struct WGLayerDesc
{
    HYP_STRUCT_BODY(WGLayerDesc);

    static const Name s_defaultName;

    HYP_FIELD()
    Name className = s_defaultName;

    HYP_FIELD()
    Name layerName;

    HYP_FIELD()
    WorldGridLayerInfo info;

    HYP_FIELD()
    Array<WGObject> objects;
};

HYP_CLASS()
class HYP_API WorldGrid final : public ObjectBase
{
    HYP_OBJECT_BODY(WorldGrid);

    friend class World;

public:
    WorldGrid();
    explicit WorldGrid(World* world);

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

private:
    void Init() override;

    void SetStreamingLayersFromDescs(Span<const WGLayerDesc> descs);
    Array<WGLayerDesc> GetStreamingLayerDescs() const;

    World* m_world;

    WorldGridState m_state;
    Array<Handle<WorldGridLayer>> m_layers;
};

} // namespace Hyperion
