/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/world_grid/WorldGridLayer.hpp>

#include <streaming/StreamingCell.hpp>

#include <core/reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class Material;
class Mesh;
class Node;

HYP_CLASS()
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
} // namespace Hyperion
