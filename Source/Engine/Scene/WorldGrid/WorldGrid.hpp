/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridState.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Scene/Entity.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/FlatSet.hpp>
#include <Core/Containers/Queue.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Threading/Task.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector2.hpp>

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
class ENGINE_API WorldGrid final : public ObjectBase
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
