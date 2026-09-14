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

#include <Core/Threading/AtomicVar.hpp>

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

enum class MaterialTextureKey : uint64;

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

    ///true once the cell's rigid body has been added to the physics world
    bool HasCollider() const;

protected:
    virtual void OnStreamStart() override final;

    virtual void OnLoaded() override final;
    virtual void OnRemoved() override final;

    Handle<Mesh> BuildMeshFromCellMeshData() const;

private:
    ///the layer has been regenerated since this cell was created if this returns true
    bool IsStale() const;

    HYP_FORCE_INLINE uint32 GetCellSize() const
    {
        return m_generationLayerInfo.cellSize;
    }

    ///true if m_cellData has heights usable for this cell's generation state, so no generation is needed
    bool HasCurrentSavedHeights() const;

    void BeginPendingGeneration();
    void EndPendingGeneration();

    void ReleaseBuildData();

    bool BuildCellMeshData(Array<float>& outGeneratedHeights);

    void RebuildMeshFull(const Handle<TerrainCellData>& cellData);
    void UpdateCollider(bool notifyPhysicsWorld);

    ///rebuilds the per-cell normal map from the full resolution grid vertices (LOD 0, skirts excluded)
    void RefreshNormalMap(Span<const TerrainVertex> gridVertices);
    void ApplyNormalMapTexture(const Handle<Texture>& normalMapTexture);

    ///binds \p texture to this cell's material instance, cloning the layer material the first time
    void BindCellMaterialTexture(MaterialTextureKey key, const Handle<Texture>& texture);

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    Handle<TerrainWorldGridLayer> m_layer;
    Handle<TerrainCellData> m_cellData;

    SharedPtr<TerrainGenerator> m_generator;
    uint64 m_cellFingerprint = 0;
    uint32 m_generationEpoch = 0;

    ///the layer's info cached as of m_generationEpoch.
    ///never read the layer's live info, it can change mid-build
    WorldGridLayerInfo m_generationLayerInfo;

    ///sim thread only
    bool m_isRemoved = false;

    ///set while counted in the editor's terrain generation task.
    ///touched by the streaming manager, streaming worker and sim threads
    AtomicVar<bool> m_isPendingGeneration { false };

    Handle<Node> m_node;
    Handle<Entity> m_entity;

    Handle<Mesh> m_mesh;

    Handle<HeightFieldPhysicsShape> m_collisionShape;

    Array<float> m_colliderHeights;

    Handle<Material> m_cellMaterial;
    Handle<Texture> m_splatTexture;
    Handle<Texture> m_normalMapTexture;

    Array<TerrainVertex> m_scratchVertices;

    ///heights generated on the streaming thread
    Array<float> m_generatedHeights;

    ///splat map bytes prepared on the streaming thread, ready for texture upload
    Array<ubyte> m_splatUploadBytes;

    ///normal map bytes prepared on the streaming thread, ready for texture upload
    Array<ubyte> m_normalMapUploadBytes;

    TerrainMeshBuilder::CellMeshData m_cellMeshData;
};
} // namespace Hyperion
