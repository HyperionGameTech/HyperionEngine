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

#include <Core/Memory/SharedPtr.hpp>

namespace Hyperion {

class Scene;
class Material;
class Mesh;
class Node;
class Entity;
class Texture;
class TerrainWorldGridLayer;
class TerrainCellData;
class TerrainGenerator;
class HeightFieldPhysicsShape;
struct TerrainGenerationState;

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
        const Handle<TerrainCellData>& cellData,
        TerrainGenerationState&& generationState);

    virtual ~TerrainStreamingCell() override;

    ///removes the cell's node and entity from the scene right away; the cell itself is unloaded later by the streaming manager
    void DetachFromScene();

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
    ///the layer has been regenerated since this cell was created if this returns true
    bool IsStale() const;

    void ReleaseBuildData();

    bool BuildCellMeshData(Array<float>& outGeneratedHeights);

    void RebuildMeshFull(const Handle<TerrainCellData>& cellData);
    void UpdateCollider(bool notifyPhysicsWorld);

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    Handle<TerrainWorldGridLayer> m_layer;
    Handle<TerrainCellData> m_cellData;

    SharedPtr<TerrainGenerator> m_generator;
    uint64 m_cellFingerprint = 0;
    uint32 m_generationEpoch = 0;

    ///sim thread only
    bool m_isRemoved = false;

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
