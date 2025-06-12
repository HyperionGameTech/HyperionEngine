/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TERRAIN_WORLD_GRID_PLUGIN_HPP
#define HYPERION_TERRAIN_WORLD_GRID_PLUGIN_HPP

#include <scene/world_grid/WorldGridPlugin.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <core/Handle.hpp>

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
    TerrainStreamingCell(WorldGrid* world_grid, const StreamingCellInfo& cell_info, const Handle<Scene>& scene, const Handle<Material>& material);
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
    virtual void OnAdded_Impl(WorldGrid* world_grid) override;

    HYP_METHOD()
    virtual void OnRemoved_Impl(WorldGrid* world_grid) override;

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cell_info) override;

    Handle<Scene> m_scene;
    Handle<Material> m_material;
};

HYP_CLASS(NoScriptBindings)
class HYP_API TerrainWorldGridPlugin : public WorldGridPlugin
{
    HYP_OBJECT_BODY(TerrainWorldGridPlugin);

public:
    TerrainWorldGridPlugin();
    virtual ~TerrainWorldGridPlugin() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

protected:
    HYP_METHOD()
    virtual void Initialize_Impl(WorldGrid* world_grid) override final;

    HYP_METHOD()
    virtual void Shutdown_Impl(WorldGrid* world_grid) override final;

    HYP_METHOD()
    virtual void Update_Impl(float delta) override final;

    HYP_METHOD()
    virtual Handle<StreamingCell> CreatePatch_Impl(WorldGrid* world_grid, const StreamingCellInfo& cell_info) override final;

    Handle<Scene> m_scene;
    Handle<Material> m_material;
};

} // namespace hyperion

#endif
