/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class WorldGrid;
class WGObject;
class StreamingCell;
struct StreamingCellInfo;
class AssetObject;
class AssetReference;

HYP_STRUCT()
struct WorldGridLayerInfo
{
    HYP_STRUCT_BODY(WorldGridLayerInfo);

    HYP_FIELD(Property = "Offset")
    Vec3f offset { 0.0f, 0.0f, 0.0f };

    HYP_FIELD(Property = "Scale")
    Vec3f scale { 1.0f, 1.0f, 1.0f };

    HYP_FIELD(Property = "Range")
    Vec2i range { -10, 10 };

    HYP_FIELD(Property = "CellSize")
    uint32 cellSize = 32;

    HYP_FIELD(Property = "MaxDistance")
    float maxDistance = 1.0f;

    HYP_FIELD(Property = "Seed")
    uint32 seed = 0;

    HYP_FIELD(Property = "Infinite")
    bool infinite = true;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(range);
        hc.Add(cellSize);
        hc.Add(maxDistance);
        hc.Add(seed);
        hc.Add(infinite);

        return hc;
    }
};

HYP_CLASS()
class ENGINE_API WorldGridLayer : public ObjectBase
{
    HYP_OBJECT_BODY(WorldGridLayer);

    friend class WorldGrid;

public:
    WorldGridLayer() = default;

    explicit WorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo = {})
        : m_name(name),
          m_layerInfo(layerInfo)
    {
    }

    WorldGridLayer(const WorldGridLayer& other) = delete;
    WorldGridLayer& operator=(const WorldGridLayer& other) = delete;

    WorldGridLayer(WorldGridLayer&& other) = delete;
    WorldGridLayer& operator=(WorldGridLayer&& other) = delete;

    virtual ~WorldGridLayer() = default;

    HYP_METHOD(Property = "Name")
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name")
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD()
    Name GetLayerClassName() const;

    HYP_METHOD(Property = "LayerInfo")
    HYP_FORCE_INLINE const WorldGridLayerInfo& GetLayerInfo() const
    {
        return m_layerInfo;
    }

    HYP_METHOD(Property = "LayerInfo")
    HYP_FORCE_INLINE virtual void SetLayerInfo(const WorldGridLayerInfo& layerInfo)
    {
        m_layerInfo = layerInfo;
    }

    HYP_METHOD()
    virtual void OnAdded(WorldGrid* worldGrid)
    {
    }

    HYP_METHOD()
    virtual void OnRemoved(WorldGrid* worldGrid)
    {
    }

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo);

    virtual void StreamPrefetch(Span<const Vec2i> cellCoords)
    {
    }

    HYP_METHOD()
    void AddStreamingObject(const AssetObject* assetObject, const Vec2i& coord);

    HYP_METHOD()
    void RemoveStreamingObject(const AssetObject* assetObject);

    HYP_METHOD()
    void EnsureStreamingObjectsRegistered();

    Delegate<void, StreamingCell*, Span<const AssetObject*>> OnStreamingObjectsLoaded;
    Delegate<void, StreamingCell*, Span<const AssetObject*>> OnStreamingObjectsUnloaded;

protected:
    Name m_name;
    WorldGridLayerInfo m_layerInfo;
    FlatMap<Vec2i, Array<AssetReference, StreamingAllocator>, StreamingAllocator> m_objectsByCoord;
};

} // namespace Hyperion
