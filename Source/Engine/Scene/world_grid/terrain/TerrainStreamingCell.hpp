/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/world_grid/WorldGridLayer.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class MaterialInstance;
class Mesh;
class Node;

HYP_CLASS()
class TerrainStreamingCell : public StreamingCell
{
    HYP_OBJECT_BODY(TerrainStreamingCell);

public:
    TerrainStreamingCell();
    TerrainStreamingCell(const StreamingCellInfo& cellInfo, const Handle<Scene>& scene, const Handle<MaterialInstance>& material);
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
    Handle<MaterialInstance> m_material;
    Handle<Node> m_node;
};
} // namespace Hyperion
