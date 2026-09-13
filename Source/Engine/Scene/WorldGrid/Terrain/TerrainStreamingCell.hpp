/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class Material;
class Mesh;
class Node;
class Entity;
class Texture;
class TerrainWorldGridLayer;
class TerrainCellData;
class HeightFieldPhysicsShape;

HYP_CLASS()
class TerrainStreamingCell : public StreamingCell
{
    HYP_OBJECT_BODY(TerrainStreamingCell);

public:
    TerrainStreamingCell();

    TerrainStreamingCell(
        const StreamingCellInfo& cellInfo,
        const Handle<Scene>& scene,
        const Handle<Material>& material,
        const Handle<TerrainWorldGridLayer>& layer,
        const Handle<TerrainCellData>& cellData);

    virtual ~TerrainStreamingCell() override;

    void RebuildMesh(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex);

    ///update cell data to apply the splat map, if one.
    void UpdateSplatMaterial(const Handle<TerrainCellData>& cellData);

    ///binds a splat map texture to this cell's material instance
    void ApplySplatTexture(const Handle<Texture>& splatTexture);

    ///re-synthesizes the auto splat weights from the cell's current heights; no-op for painted cells
    void RefreshAutoSplat();

    void RebuildPickBVH();

protected:
    virtual void OnStreamStart() override final;

    virtual void OnLoaded() override final;
    virtual void OnRemoved() override final;

    Handle<Mesh> BuildMeshFromCellMeshData() const;

private:
    bool BuildCellMeshData(Array<float>& outGeneratedHeights);

    void RebuildMeshFull(const Handle<TerrainCellData>& cellData);
    void UpdateCollider(bool notifyPhysicsWorld);

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    Handle<TerrainWorldGridLayer> m_layer;
    Handle<TerrainCellData> m_cellData;
    Handle<Node> m_node;
    Handle<Entity> m_entity;

    Handle<Mesh> m_mesh;

    Handle<HeightFieldPhysicsShape> m_collisionShape;

    Array<float> m_colliderHeights;

    Handle<Material> m_cellMaterial;
    Handle<Texture> m_splatTexture;

    Array<SimpleVertex> m_scratchVertices;

    ///heights generated on the streaming thread
    Array<float> m_generatedHeights;

    ///splat map bytes prepared on the streaming thread, ready for texture upload
    Array<ubyte> m_splatUploadBytes;

    TerrainMeshBuilder::CellMeshData m_cellMeshData;
};
} // namespace Hyperion
