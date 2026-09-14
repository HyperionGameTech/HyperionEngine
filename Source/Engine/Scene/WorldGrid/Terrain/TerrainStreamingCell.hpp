/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>
#include <Scene/WorldGrid/Terrain/TerrainQuadtree.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Transform.hpp>

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
struct MeshComponent;
struct TerrainPatchComponent;

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

    ///removes the cell's node and entities from the scene right away; the cell itself is unloaded later by the streaming manager
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

    ///sim thread - picks the patches drawn for \p viewpoints, and queues building patches that come into range and
    ///releasing ones that leave it
    void UpdateLodSelection(Span<const Vec3f> viewpoints);

    ///sim thread - points \p meshComponent at the patch mesh if it's drawn after the last UpdateLodSelection(), and updates the
    ///morph band. Returns true if the entity's render proxy needs updating; \p outDrawnMeshChanged is set if the mesh was swapped
    bool ApplyPatchLod(
        Span<const Vec3f> viewpoints,
        MeshComponent& meshComponent,
        TerrainPatchComponent& patchComponent,
        bool& outDrawnMeshChanged) const;

protected:
    virtual void OnStreamStart() override final;

    virtual void OnLoaded() override final;
    virtual void OnRemoved() override final;

private:
    struct TerrainPatch
    {
        ///built while the patch's node is in range of a viewpoint
        Handle<Mesh> mesh;
        Handle<Entity> entity;

        bool isBuildQueued = false;

        ///set by UpdateLodSelection() - never set without a mesh
        bool isDrawn = false;
    };

    struct TerrainPatchBuild
    {
        uint32 patchIndex = 0;
        TerrainPatchMeshData meshData;
    };

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

    ///fills m_paddedHeights from the saved heights if they're current, otherwise generates them - returns true if generated
    bool LoadOrGeneratePaddedHeights();

    ///includes the full resolution heights of the whole tile
    BoundingBox ComputeTileLocalBounds() const;

    ///the tile's world transform, shared by its collider and patch entities
    Transform ComputeTileTransform() const;

    const Handle<Material>& GetTileMaterial() const;

    ///streaming thread - reads the Terrain.Lod cvars for the layout
    void ResetQuadtree();

    void UpdateNodeHeightBounds();

    BoundingBox GetNodeWorldBounds(uint32 nodeIndex) const;
    BoundingBox GetPatchWorldBounds(uint32 patchIndex) const;

    void ComputeNodeDistances(Span<const Vec3f> viewpoints, Array<float>& outNodeDistances) const;

    float GetLodRange(uint8 level) const;
    float GetLodMorphStart(uint8 level) const;

    bool IsNodeResident(uint32 nodeIndex) const;

    ///nodes are built a little before they come into range and released well after, so a node near its boundary doesn't
    ///rebuild back and forth. The top node is always wanted
    bool IsNodeWanted(uint32 nodeIndex, float nodeDistance) const;

    ///streaming thread - the top node plus any node already wanted for the layer's last LOD viewpoints
    void BuildInitialPatchMeshData();

    void QueuePatchBuilds(Array<uint32>&& patchIndices);
    void ApplyPatchBuilds(Array<TerrainPatchBuild>&& patchBuilds, uint32 buildGeneration);

    ///sim thread, deferred - releases patches that UpdateLodSelection() found out of range, unless they're drawn again by then
    void ReleaseUnwantedPatches(Array<uint32>&& patchIndices);

    void CreatePatch(uint32 patchIndex, const TerrainPatchMeshData& meshData);
    void ReleasePatch(uint32 patchIndex);

    ///rebuilds patches whose morph targets depend on heights in [minVertex, maxVertex], from m_paddedHeights
    void RebuildPatchesInRegion(const Vec2i& minVertex, const Vec2i& maxVertex);

    void UpdateCollider(bool notifyPhysicsWorld);

    ///rebuilds the per-cell normal map from the full resolution heights
    void RefreshNormalMap();
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

    ///holds the collider - the drawn geometry is on the patch entities
    Handle<Entity> m_entity;

    Handle<HeightFieldPhysicsShape> m_collisionShape;

    ///(cellSize + 2 * CellPadding)^2 - patches, the collider and the normal map are all built from these
    Array<float> m_paddedHeights;

    TerrainQuadtreeLayout m_quadtreeLayout;
    Array<float> m_nodeMinHeights;
    Array<float> m_nodeMaxHeights;

    ///nodes in range at the last selection, for hysteresis
    Array<uint8> m_nodeInRange;

    Array<TerrainPatch> m_patches;

    ///bumped whenever the heights change, so async patch builds from older heights are dropped
    uint32 m_patchBuildGeneration = 0;

    ///built on the streaming thread, turned into patch entities by OnLoaded()
    Array<TerrainPatchBuild> m_initialPatchBuilds;

    Handle<Material> m_cellMaterial;
    Handle<Texture> m_splatTexture;
    Handle<Texture> m_normalMapTexture;

    ///set on the streaming thread when m_paddedHeights had to be generated, so OnLoaded() stores them
    bool m_hasGeneratedHeights = false;

    ///splat map bytes prepared on the streaming thread, ready for texture upload
    Array<ubyte> m_splatUploadBytes;

    ///normal map bytes prepared on the streaming thread, ready for texture upload
    Array<ubyte> m_normalMapUploadBytes;
};
} // namespace Hyperion
