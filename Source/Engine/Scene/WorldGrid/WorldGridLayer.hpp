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

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class WorldGrid;
class WGObject;
class StreamingCell;
struct StreamingCellInfo;
class AssetObject;
class AssetReference;

HYP_STRUCT(Size = 48)
struct WorldGridLayerInfo
{
    HYP_STRUCT_BODY(WorldGridLayerInfo);

    HYP_FIELD()
    Vec3f offset { 0.0f, 0.0f, 0.0f };

    HYP_FIELD()
    Vec3f scale { 1.0f, 1.0f, 1.0f };

    HYP_FIELD()
    uint32 cellSize = 32;

    HYP_FIELD()
    float maxDistance = 5.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(cellSize);
        hc.Add(maxDistance);

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

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const WorldGridLayerInfo& GetLayerInfo() const
    {
        return m_layerInfo;
    }

    HYP_METHOD(Scriptable)
    void OnAdded(WorldGrid* worldGrid);

    HYP_METHOD(Scriptable)
    void OnRemoved(WorldGrid* worldGrid);

    HYP_METHOD(Scriptable)
    Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo);

    HYP_METHOD()
    void AddStreamingObject(const AssetObject* assetObject, const Vec2i& coord);

    HYP_METHOD()
    void RemoveStreamingObject(const AssetObject* assetObject);

    Delegate<void, StreamingCell*, Array<const AssetObject*>> OnStreamingObjectsLoaded;
    Delegate<void, StreamingCell*, Array<const AssetObject*>> OnStreamingObjectsUnloaded;

protected:
    HYP_METHOD(Scriptable)
    virtual void Init() override;

    HYP_METHOD()
    virtual void OnAdded_Impl(WorldGrid* worldGrid)
    {
    }

    HYP_METHOD()
    virtual void OnRemoved_Impl(WorldGrid* worldGrid)
    {
    }

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo);

    Name m_name;
    WorldGridLayerInfo m_layerInfo;
    FlatMap<Vec2i, Array<AssetReference, DynamicAllocator>> m_objectsByCoord;

private:
    HYP_METHOD()
    virtual void Init_Impl()
    {
        SetReady(true);
    }
};

} // namespace Hyperion
