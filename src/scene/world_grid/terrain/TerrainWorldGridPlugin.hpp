/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/world_grid/WorldGridLayer.hpp>

#include <core/reflection/Handle.hpp>

namespace hyperion {

class Material;
class Mesh;
class Scene;
class Node;
class NoiseCombinator;

HYP_CLASS(NoScriptBindings)
class HYP_API TerrainStreamingCell : public StreamingCell
{
    HYP_OBJECT_BODY(TerrainStreamingCell);

public:
    TerrainStreamingCell();
    TerrainStreamingCell(const StreamingCellInfo& cellInfo, const Handle<Scene>& scene, const Handle<Material>& material);
    virtual ~TerrainStreamingCell() override;

protected:
    HYP_METHOD()
    virtual void OnStreamStart_Impl() override final;

    HYP_METHOD()
    virtual void OnLoaded_Impl() override final;

    HYP_METHOD()
    virtual void OnRemoved_Impl() override final;

    Handle<Scene> m_scene;
    Handle<Mesh> m_mesh;
    Handle<Material> m_material;
    Handle<Node> m_node;
};

HYP_CLASS(NoScriptBindings)
class HYP_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

public:
    TerrainWorldGridLayer();
    virtual ~TerrainWorldGridLayer() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

protected:
    HYP_METHOD()
    virtual void Init() override;

    HYP_METHOD()
    virtual void OnAdded_Impl(WorldGrid* worldGrid) override;

    HYP_METHOD()
    virtual void OnRemoved_Impl(WorldGrid* worldGrid) override;

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo) override;

    Handle<Scene> m_scene;
    Handle<Material> m_material;
};

} // namespace hyperion
