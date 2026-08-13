/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class Material;
class Mesh;
class Node;

HYP_CLASS()
class TerrainStreamingCell : public StreamingCell
{
    HYP_OBJECT_BODY(TerrainStreamingCell);

public:
    TerrainStreamingCell();

    TerrainStreamingCell(
        const StreamingCellInfo& cellInfo,
        const Handle<Scene>& scene,
        const Handle<Mesh>& mesh,
        const Handle<Material>& material);

    virtual ~TerrainStreamingCell() override;

protected:
    virtual void OnStreamStart() override final;

    virtual void OnLoaded() override final;
    virtual void OnRemoved() override final;

    Handle<Scene> m_scene;
    Handle<Mesh> m_mesh;
    Handle<Material> m_material;
    Handle<Node> m_node;
};
} // namespace Hyperion
