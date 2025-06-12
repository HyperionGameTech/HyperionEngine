/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_LAYER_HPP
#define HYPERION_WORLD_GRID_LAYER_HPP

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
    Vec2u grid_size { 64, 64 };

    HYP_FIELD(Property = "CellSize", Serialize = true)
    Vec3u cell_size { 32, 32, 32 };

    HYP_FIELD(Property = "Offset", Serialize = true)
    Vec3f offset { 0.0f, 0.0f, 0.0f };

    HYP_FIELD(Property = "Scale", Serialize = true)
    Vec3f scale { 1.0f, 1.0f, 1.0f };

    HYP_FIELD(Property = "MaxDistance", Serialize = true)
    float max_distance = 2.5f;

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
class HYP_API WorldGridLayer : public HypObject<WorldGridLayer>
{
    HYP_OBJECT_BODY(WorldGridLayer);

public:
    WorldGridLayer() = default;

    WorldGridLayer(const WorldGridLayerInfo& layer_info)
        : m_layer_info(layer_info)
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
        return m_layer_info;
    }

    HYP_METHOD(Scriptable)
    void OnAdded(WorldGrid* world_grid);

    HYP_METHOD(Scriptable)
    void OnRemoved(WorldGrid* world_grid);

    HYP_METHOD(Scriptable)
    Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cell_info);

protected:
    HYP_METHOD(Scriptable)
    virtual void Init() override;

    HYP_METHOD()
    virtual void OnAdded_Impl(WorldGrid* world_grid)
    {
    }

    HYP_METHOD()
    virtual void OnRemoved_Impl(WorldGrid* world_grid)
    {
    }

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cell_info)
    {
        return CreateObject<StreamingCell>(nullptr, cell_info);
    }

    HYP_METHOD(Scriptable)
    virtual WorldGridLayerInfo CreateLayerInfo() const;

    WorldGridLayerInfo m_layer_info;

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
        m_layer_info = CreateLayerInfo();

        SetReady(true);
    }
};

} // namespace hyperion

#endif
