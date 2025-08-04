/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>

#include <streaming/StreamingCell.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_STRUCT(Size = 80)
struct WorldGridLayerInfo
{
    HYP_FIELD(Property = "GridSize", Serialize = true)
    Vec2u gridSize { 64, 64 };

    HYP_FIELD(Property = "CellSize", Serialize = true)
    Vec3u cellSize { 32, 32, 32 };

    HYP_FIELD(Property = "Offset", Serialize = true)
    Vec3f offset { 0.0f, 0.0f, 0.0f };

    HYP_FIELD(Property = "Scale", Serialize = true)
    Vec3f scale { 1.0f, 1.0f, 1.0f };

    HYP_FIELD(Property = "MaxDistance", Serialize = true)
    float maxDistance = 2.5f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(gridSize);
        hc.Add(cellSize);
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(maxDistance);

        return hc;
    }
};

HYP_CLASS()
class HYP_API WorldGridLayer : public HypObjectBase
{
    HYP_OBJECT_BODY(WorldGridLayer);

public:
    WorldGridLayer() = default;

    WorldGridLayer(const WorldGridLayerInfo& layerInfo)
        : m_layerInfo(layerInfo)
    {
    }

    WorldGridLayer(const WorldGridLayer& other) = delete;
    WorldGridLayer& operator=(const WorldGridLayer& other) = delete;
    WorldGridLayer(WorldGridLayer&& other) = delete;
    WorldGridLayer& operator=(WorldGridLayer&& other) = delete;
    virtual ~WorldGridLayer() = default;

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
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo)
    {
        return CreateObject<StreamingCell>(cellInfo);
    }

    HYP_METHOD(Scriptable)
    virtual WorldGridLayerInfo CreateLayerInfo() const;

    WorldGridLayerInfo m_layerInfo;

private:
    HYP_METHOD()
    WorldGridLayerInfo CreateLayerInfo_Impl() const
    {
        // default implementation returns default object
        return WorldGridLayerInfo {};
    }

    HYP_METHOD()
    virtual void Init_Impl()
    {
        m_layerInfo = CreateLayerInfo();

        SetReady(true);
    }
};

} // namespace hyperion

