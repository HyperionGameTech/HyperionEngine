/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellTextures.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainErosion.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TerrainCellComponent.hpp>
#include <Scene/Components/TerrainPatchComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Guarded.hpp>
#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Scene/WorldGrid/Terrain/TerrainGenerationEditorTask.hpp>

#include <TerrainStreamingCell.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static CVar<bool> g_cvTerrainGrass("Terrain.Grass", true);

///how far from a viewpoint grass is planted
static CVar<float> g_cvTerrainGrassRange("Terrain.Grass.Range", 80.0f);
///scales how many patches every ground cover layer plants
static CVar<float> g_cvTerrainGrassDensity("Terrain.Grass.Density", 1.0f);
///past this, tiles are planted with fewer, wider patches
static CVar<float> g_cvTerrainGrassNearRange("Terrain.Grass.NearRange", 30.0f);
///how much further apart, and wider, the patches of far tiles are
static constexpr float s_grassFarStretch = 1.8f;


///the physics collider always uses the full-resolution grid, regardless of the mesh LODs in memory
static void ExtractColliderHeights(Span<const float> paddedHeights, uint32 cellSize, Array<float>& outHeights)
{
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    Assert(paddedHeights.Size() == size_t(paddedSize) * size_t(paddedSize), "Padded heights have unexpected size");

    outHeights.Resize(size_t(cellSize) * size_t(cellSize));

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            outHeights[size_t(z) * cellSize + x] = paddedHeights[size_t(z + TerrainGenerator::CellPadding) * paddedSize + (x + TerrainGenerator::CellPadding)];
        }
    }
}

static void BuildPatchMeshDescAndDataView(const TerrainPatchMeshData& patchMeshData, MeshDesc& outMeshDesc, MeshDataView& outMeshData)
{
    outMeshDesc.meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple | VT_UV1>;
    outMeshDesc.lods[0].numIndices = uint32(patchMeshData.indices.Size());
    outMeshDesc.lods[0].numVertices = uint32(patchMeshData.vertices.Size());

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(patchMeshData.vertices.Data());
    vertexArrayView.vertexCount = patchMeshData.vertices.Size();
    vertexArrayView.layoutDesc = outMeshDesc.meshAttributes.inputLayout;

    outMeshData.vertices[0] = vertexArrayView;
    outMeshData.indices[0] = patchMeshData.indices.ToByteView();
}

static float DistanceToBounds(const Vec3f& point, const BoundingBox& bounds)
{
    const Vec3f closestPoint {
        MathUtil::Clamp(point.x, bounds.min.x, bounds.max.x),
        MathUtil::Clamp(point.y, bounds.min.y, bounds.max.y),
        MathUtil::Clamp(point.z, bounds.min.z, bounds.max.z)
    };

    return (point - closestPoint).Length();
}

#pragma region TerrainStreamingCell

TerrainStreamingCell::TerrainStreamingCell()
    : StreamingCell()
{
}

TerrainStreamingCell::TerrainStreamingCell(
    const StreamingCellInfo& cellInfo,
    const Handle<Scene>& scene,
    const Handle<Material>& material,
    const Handle<TerrainWorldGridLayer>& layer,
    const Handle<TerrainCellData>& cellData,
    TerrainGenerationState&& generationState)
    : StreamingCell(cellInfo),
      m_scene(scene),
      m_material(material),
      m_layer(layer),
      m_cellData(cellData),
      m_generator(std::move(generationState.generator)),
      m_cellFingerprint(generationState.cellFingerprint),
      m_generationEpoch(generationState.epoch),
      m_generationLayerInfo(generationState.layerInfo)
{
    AssertDebug(m_cellInfo.extent.x == m_generationLayerInfo.cellSize, "Cell info must be built from the generation state's layer info");

    // counted as soon as it's queued, so the editor task shows the whole backlog rather than just the cells being finished
    if (!IsStale() && !HasCurrentSavedHeights())
    {
        BeginPendingGeneration();
    }
}

TerrainStreamingCell::~TerrainStreamingCell() = default;

bool TerrainStreamingCell::IsStale() const
{
    return !m_generator || !m_layer.IsValid() || !m_layer->IsGenerationCurrent(m_generationEpoch);
}

bool TerrainStreamingCell::HasCurrentSavedHeights() const
{
    if (!m_cellData.IsValid() || !m_cellData->HasHeights())
    {
        return false;
    }

    auto cellDataReadScope = m_cellData->GetReadScope();

    return TerrainWorldGridLayer::AreCellHeightsCurrent(*m_cellData, GetCellSize(), m_cellFingerprint);
}

void TerrainStreamingCell::BeginPendingGeneration()
{
#ifdef HYP_EDITOR
    if (!m_isPendingGeneration.Exchange(true, MemoryOrder::ACQUIRE_RELEASE))
    {
        TerrainGenerationEditorTask::OnGenerationQueued();
    }
#endif
}

void TerrainStreamingCell::EndPendingGeneration()
{
#ifdef HYP_EDITOR
    if (m_isPendingGeneration.Exchange(false, MemoryOrder::ACQUIRE_RELEASE))
    {
        TerrainGenerationEditorTask::OnGenerationFinished();
    }
#endif
}

void TerrainStreamingCell::ReleaseBuildData()
{
    EndPendingGeneration();

    m_hasGeneratedHeights = false;
    m_splatTexture.Reset();
    m_normalMapTexture.Reset();
    m_initialPatchBuilds = Array<TerrainPatchBuild>();
}

bool TerrainStreamingCell::LoadOrGeneratePaddedHeights()
{
    HYP_SCOPE;

    const uint32 cellSize = GetCellSize();
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    if (m_cellData.IsValid() && m_cellData->HasHeights())
    {
        auto cellDataReadScope = m_cellData->GetReadScope();

        const TerrainCellData& cellData = *m_cellData;
        const Span<const float> savedHeights = cellData.GetHeights();

        if (TerrainWorldGridLayer::AreCellHeightsCurrent(cellData, cellSize, m_cellFingerprint) && savedHeights.Size() == size_t(paddedSize) * size_t(paddedSize))
        {
            m_paddedHeights.Resize(savedHeights.Size());
            Memory::Copy(m_paddedHeights.Data(), savedHeights.Data(), savedHeights.Size() * sizeof(float));

            // cells sculpted before erosion masks were saved have none
            const ConstByteView savedErosionMasks = cellData.GetErosionMasks();

            if (savedErosionMasks.Size() == size_t(cellSize) * size_t(cellSize) * TerrainErosionMasks::NumChannels)
            {
                m_erosionMasks.Resize(savedErosionMasks.Size());
                Memory::Copy(m_erosionMasks.Data(), savedErosionMasks.Data(), savedErosionMasks.Size());
            }
            else
            {
                m_erosionMasks.Clear();
            }

            return false;
        }

        if (cellData.isSculpted)
        {
            HYP_LOG(WorldGrid, Warning,
                "Cell {} has sculpted heights that can't be used (loaded {} heights, extent {}, layer cell size {}) - regenerating, sculpt edits for this cell will be lost when saved",
                m_cellInfo.coord,
                savedHeights.Size(),
                cellData.extent,
                cellSize);
        }
        else if (TerrainWorldGridLayer::AreCellHeightsCurrent(cellData, cellSize, m_cellFingerprint))
        {
            HYP_LOG(WorldGrid, Warning,
                "Cell {} has current saved heights in '{}' but only {} of {} could be paged in - regenerating",
                m_cellInfo.coord,
                cellData.GetName(),
                savedHeights.Size(),
                size_t(paddedSize) * size_t(paddedSize));
        }
    }

    TerrainWorldGridLayer::GenerateCellPaddedHeights(*m_generator, m_generationLayerInfo, m_cellInfo.coord, m_paddedHeights, &m_erosionMasks);

    return true;
}

void TerrainStreamingCell::OnStreamStart()
{
    HYP_SCOPE;

    Assert(m_layer.IsValid(), "Invalid terrain layer!");

    if (IsStale())
    {
        // regenerated while this cell was queued - skip the (expensive) build, OnLoaded() discards it
        return;
    }

    m_hasGeneratedHeights = LoadOrGeneratePaddedHeights();

    if (m_hasGeneratedHeights)
    {
        // already counted, unless the saved heights looked usable at creation but couldn't be paged in
        BeginPendingGeneration();
    }
    else
    {
        EndPendingGeneration();
    }

    const uint32 cellSize = GetCellSize();

    ResetQuadtree();
    BuildInitialPatchMeshData();

    Array<ubyte> scratchBuffer;

    {
        TerrainCellTextures::PrepareNormalMapBytes(m_paddedHeights, m_erosionMasks, cellSize, m_cellInfo.scale, scratchBuffer);

        if (scratchBuffer.Any())
        {
            m_normalMapTexture = TerrainCellTextures::CreateCellTexture(NAME_FMT("TerrainCellNormalMap_{}", m_cellInfo.coord), cellSize, scratchBuffer);
        }

        scratchBuffer.Resize(0);
    }

    bool hasSplatBytes = false;

    if (m_cellData.IsValid() && m_cellData->HasSplatMap())
    {
        hasSplatBytes = TerrainCellTextures::PreparePaintedSplatBytes(m_cellData, m_cellInfo.coord, cellSize, scratchBuffer);

        if (!hasSplatBytes)
        {
            HYP_LOG(WorldGrid, Warning, "Cell {} splat data could not be loaded!", m_cellInfo.coord);
        }
    }

    if (!hasSplatBytes && m_generator->GetParams().autoPaintSplats)
    {
        hasSplatBytes = TerrainCellTextures::PrepareAutoSplatBytes(*m_generator, m_cellInfo, m_paddedHeights, m_erosionMasks, scratchBuffer);
    }

    if (hasSplatBytes)
    {
        SetSplatWeights(scratchBuffer, /* rowsFlipped */ true);
        
        m_splatTexture = TerrainCellTextures::CreateSplatTexture(m_cellInfo.coord, cellSize, scratchBuffer);
    }

    if (m_cellData.IsValid() && m_cellData->HasGroundCoverPaint())
    {
        auto readScope = m_cellData->GetReadScope();

        SetGroundCoverPaint(*m_cellData);
    }
}

void TerrainStreamingCell::OnLoaded()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_scene.IsValid(), "Invalid scene!");
    Assert(m_material.IsValid(), "Invalid material!");
    Assert(m_layer.IsValid(), "Invalid terrain layer!");

    if (m_isRemoved || IsStale())
    {
        // already unloaded, or built with a replaced generator: never spawn it or persist its heights
        ReleaseBuildData();

        m_paddedHeights = Array<float>();
        m_erosionMasks = Array<ubyte>();

        return;
    }

    if (m_hasGeneratedHeights)
    {
#ifdef HYP_EDITOR
        m_cellData = m_layer->StoreGeneratedCellHeights(m_cellInfo.coord, m_paddedHeights, m_erosionMasks);
#endif

        m_hasGeneratedHeights = false;
    }

    EndPendingGeneration();

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        // The cell was loaded, but the Scene was likely removed from the World.
        // Can happen when we enter/exit simulation mode from the editor.

        // Accept it and move on

        // Ensure these are unset, since we use m_node's existance to determine if we were added to the layer or not.

        m_node.Reset();
        m_entity.Reset();

        ReleaseBuildData();

        return;
    }

    const uint32 cellSize = GetCellSize();


    HYP_LOG(WorldGrid, Verbose, "Creating terrain tile at coord {} with extent {} and scale {}, bounds: {}", m_cellInfo.coord, m_cellInfo.extent, m_cellInfo.scale, m_cellInfo.bounds);

    const Transform transform = ComputeTileTransform();

    EntityInitInfo entityInitInfo {};
    entityInitInfo.bvhDepth = 0; // don't build bvhs to save load times

    m_entity = MakeHandle<Entity>(NAME_FMT("TerrainTile_{}_Entity", m_cellInfo.coord), entityInitInfo);
    m_entity->SetLocalBounds(ComputeTileLocalBounds());
    m_entity->SetIsStatic(true);

    entityManager->AddExistingEntity(m_entity);

    entityManager->GetComponent<TransformComponent>(m_entity) = TransformComponent {
        transform.GetTranslation(),
        transform.GetRotation(),
        transform.GetScale()
    };

    entityManager->AddComponent<TerrainCellComponent>(m_entity, TerrainCellComponent {
        .layer = m_layer.ToWeak(),
        .cell = WeakHandleFromThis()
    });

    m_collisionShape = MakeHandle<HeightFieldPhysicsShape>(NAME_FMT("TerrainCellCollider_{}", m_cellInfo.coord));
    InitObject(m_collisionShape);

    UpdateCollider(false /* notifyPhysicsWorld */);

    entityManager->AddComponent<RigidBodyComponent>(m_entity, RigidBodyComponent {
        .shape = m_collisionShape
    });

    m_node = m_scene->GetRoot()->AddChild();
    m_node->SetName(NAME_FMT("TerrainTile_{}", m_cellInfo.coord));
    m_node->AddChild(m_entity);
    m_node->SetLocalTransform(transform);
    m_node->SetIsStatic(true);

    for (const TerrainPatchBuild& patchBuild : m_initialPatchBuilds)
    {
        CreatePatch(patchBuild.patchIndex, patchBuild.meshData);
    }

    m_initialPatchBuilds = Array<TerrainPatchBuild>();

    if (m_normalMapTexture.IsValid())
    {
        ApplyNormalMapTexture(m_normalMapTexture);
    }

    if (m_splatTexture.IsValid())
    {
        ApplySplatTexture(m_splatTexture);
    }

    m_layer->RegisterLoadedCell(m_cellInfo.coord, WeakHandleFromThis());

    // selects and assigns the drawn patches now, so the tile doesn't pop in a frame after it's added
    const Array<Vec3f> viewpoints = m_layer->GetLodViewpoints();

    UpdateLodSelection(viewpoints);

    for (const TerrainPatch& patch : m_patches)
    {
        const Handle<Entity>& patchEntity = patch.entity;

        if (!patchEntity.IsValid())
        {
            continue;
        }

        MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(patchEntity);
        TerrainPatchComponent* patchComponent = entityManager->TryGetComponent<TerrainPatchComponent>(patchEntity);

        if (!meshComponent || !patchComponent)
        {
            continue;
        }

        bool drawnMeshChanged = false;

        if (ApplyPatchLod(*meshComponent, *patchComponent, drawnMeshChanged))
        {
            patchEntity->SetNeedsRenderProxyUpdate();
        }
    }
}

void TerrainStreamingCell::OnRemoved()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_isRemoved = true;

    // the cell may be removed before it ever finished loading
    ReleaseBuildData();

    if (m_layer.IsValid())
    {
        m_layer->UnregisterLoadedCell(m_cellInfo.coord, this);
    }

    DetachFromScene();

    m_splatTexture.Reset();
    m_normalMapTexture.Reset();
    m_cellMaterial.Reset();

    m_collisionShape.Reset();
    m_paddedHeights = Array<float>();
    m_erosionMasks = Array<ubyte>();
    m_splatWeights = Array<ubyte>();
    m_groundCoverPaint = Array<ubyte>();
    m_groundCoverPaintLayers = Array<Name>();
    m_grassHeightsSnapshot.Reset();
    m_grassSplatWeightsSnapshot.Reset();
    m_grassPaintSnapshot.Reset();
}

void TerrainStreamingCell::DetachFromScene()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_node.IsValid())
    {
        return;
    }

    // the collider and patch entities are children of the tile node, so they leave with it
    m_node->Remove(/* moveToDetached */ false);
    m_node.Reset();

    m_entity.Reset();

    for (TerrainPatch& patch : m_patches)
    {
        patch = TerrainPatch {};
    }

    // grass tiles aren't in the node hierarchy, so they're removed on their own
    ReleaseAllGrass();
    m_grassTiles.Clear();
}

void TerrainStreamingCell::UpdateSplatMaterial(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    const uint32 cellSize = GetCellSize();

    if (!cellData.IsValid() || !cellData->HasSplatMap() || !m_material.IsValid())
    {
        return;
    }

    Handle<Texture> splatTexture = TerrainCellTextures::BuildPaintedSplatTexture(cellData, m_cellInfo.coord, cellSize);

    if (!splatTexture.IsValid())
    {
        return;
    }

    ApplySplatTexture(splatTexture);

    {
        auto readScope = cellData->GetReadScope();

        const ConstByteView splatData = static_cast<const TerrainCellData&>(*cellData).GetSplatMap();

        Array<ubyte> splatBytes;
        splatBytes.Resize(splatData.Size());
        Memory::Copy(splatBytes.Data(), splatData.Data(), splatData.Size());

        SetSplatWeights(splatBytes, /* rowsFlipped */ false);
    }

    InvalidateGrass(minVertex, maxVertex);
}

void TerrainStreamingCell::UpdateGroundCoverPaint(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_cellData = cellData;

    if (!cellData.IsValid())
    {
        return;
    }

    {
        auto readScope = cellData->GetReadScope();

        SetGroundCoverPaint(*cellData);
    }

    InvalidateGrass(minVertex, maxVertex);
}

void TerrainStreamingCell::RefreshAutoSplat()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_layer.IsValid() || !m_entity.IsValid())
    {
        return;
    }

    // Painted splat data takes priority over synthesized weights.
    if (m_cellData.IsValid() && m_cellData->HasSplatMap())
    {
        return;
    }

    if (!m_generator || !m_generator->GetParams().autoPaintSplats)
    {
        return;
    }

    Array<ubyte> splatUploadBytes;

    if (!TerrainCellTextures::PrepareAutoSplatBytes(*m_generator, m_cellInfo, m_paddedHeights, m_erosionMasks, splatUploadBytes))
    {
        return;
    }

    ApplySplatTexture(TerrainCellTextures::CreateSplatTexture(m_cellInfo.coord, GetCellSize(), splatUploadBytes));

    SetSplatWeights(splatUploadBytes, /* rowsFlipped */ true);

    InvalidateGrass(Vec2i(0, 0), Vec2i(int32(GetCellSize()) - 1, int32(GetCellSize()) - 1));
}

void TerrainStreamingCell::ApplySplatTexture(const Handle<Texture>& splatTexture)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");

    if (!splatTexture.IsValid() || !m_material.IsValid())
    {
        return;
    }

    m_splatTexture = splatTexture;

    BindCellMaterialTexture(MaterialTextureKey::TerrainSplatMap, m_splatTexture);
}

void TerrainStreamingCell::ApplyNormalMapTexture(const Handle<Texture>& normalMapTexture)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_entity.IsValid(), "Cell has not finished loading yet");

    if (!normalMapTexture.IsValid() || !m_material.IsValid())
    {
        return;
    }

    m_normalMapTexture = normalMapTexture;

    BindCellMaterialTexture(MaterialTextureKey::TerrainNormalMap, m_normalMapTexture);
}

void TerrainStreamingCell::RefreshNormalMap()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 cellSize = GetCellSize();

    Array<ubyte> normalMapBytes;
    TerrainCellTextures::PrepareNormalMapBytes(m_paddedHeights, m_erosionMasks, cellSize, m_cellInfo.scale, normalMapBytes);

    ApplyNormalMapTexture(TerrainCellTextures::CreateCellTexture(NAME_FMT("TerrainCellNormalMap_{}", m_cellInfo.coord), cellSize, normalMapBytes));
}

void TerrainStreamingCell::BindCellMaterialTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_cellMaterial.IsValid())
    {
        m_cellMaterial = m_material->Clone();
        m_cellMaterial->SetName(NAME_FMT("TerrainCellMaterial_{}", m_cellInfo.coord));
        InitObject(m_cellMaterial);
    }

    m_cellMaterial->SetTexture(key, texture);

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        return;
    }

    for (const TerrainPatch& patch : m_patches)
    {
        if (!patch.entity.IsValid())
        {
            continue;
        }

        if (MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(patch.entity))
        {
            meshComponent->material = m_cellMaterial;
        }

        patch.entity->SetNeedsRenderProxyUpdate();
    }

    // the replaced texture is still referenced by static shadow views that skipped collection
    m_scene->MarkStaticRenderResourcesChanged();

    m_entity->MarkDirty();
}

void TerrainStreamingCell::RebuildMesh(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    Vec2i rebuildMinVertex = minVertex;
    Vec2i rebuildMaxVertex = maxVertex;

    if (LoadOrGeneratePaddedHeights())
    {
#ifdef HYP_EDITOR
        m_cellData = m_layer->StoreGeneratedCellHeights(m_cellInfo.coord, m_paddedHeights, m_erosionMasks);
#endif

        // every patch was built from the heights that just got replaced, not only the brushed ones
        rebuildMinVertex = Vec2i(0, 0);
        rebuildMaxVertex = Vec2i(int32(GetCellSize()) - 1, int32(GetCellSize()) - 1);
    }

    RebuildPatchesInRegion(rebuildMinVertex, rebuildMaxVertex);

    m_grassHeightsSnapshot.Reset();
    InvalidateGrass(rebuildMinVertex, rebuildMaxVertex);

    RefreshNormalMap();

    m_entity->SetLocalBounds(ComputeTileLocalBounds());

    UpdateCollider(true /* notifyPhysicsWorld */);
}

void TerrainStreamingCell::UpdateCollider(bool notifyPhysicsWorld)
{
    HYP_SCOPE;

    if (!m_collisionShape.IsValid() || !m_layer.IsValid() || m_paddedHeights.Empty())
    {
        return;
    }

    Array<float> colliderHeights;
    ExtractColliderHeights(m_paddedHeights, GetCellSize(), colliderHeights);

    m_collisionShape->SetHeights(colliderHeights, GetCellSize());

    if (!notifyPhysicsWorld)
    {
        return;
    }

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        return;
    }

    entityManager->AddTag<EntityTag::UpdatePhysicsShape>(m_entity);
}

bool TerrainStreamingCell::HasCollider() const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_entity.IsValid() || m_entity->GetEntityManager() == nullptr)
    {
        return false;
    }

    // PhysicsSystem creates the rigid body as it adds it to the physics world
    const RigidBodyComponent* rigidBodyComponent = m_entity->TryGetComponent<RigidBodyComponent>();

    return rigidBodyComponent != nullptr && rigidBodyComponent->rigidBody.IsValid();
}

BoundingBox TerrainStreamingCell::ComputeTileLocalBounds() const
{
    const float tileExtent = float(GetCellSize() - 1);

    float minHeight = 0.0f;
    float maxHeight = 0.0f;

    if (m_paddedHeights.Any())
    {
        minHeight = MathUtil::Infinity<float>();
        maxHeight = -MathUtil::Infinity<float>();

        for (float height : m_paddedHeights)
        {
            minHeight = MathUtil::Min(minHeight, height);
            maxHeight = MathUtil::Max(maxHeight, height);
        }
    }

    return BoundingBox(Vec3f(0.0f, minHeight, 0.0f), Vec3f(tileExtent, maxHeight, tileExtent));
}

Transform TerrainStreamingCell::ComputeTileTransform() const
{
    Transform transform;
    transform.SetTranslation(m_cellInfo.bounds.min);
    transform.SetScale(m_cellInfo.scale);

    return transform;
}

const Handle<Material>& TerrainStreamingCell::GetTileMaterial() const
{
    return m_cellMaterial.IsValid() ? m_cellMaterial : m_material;
}

#pragma region Quadtree

void TerrainStreamingCell::ResetQuadtree()
{
    HYP_SCOPE;

    m_quadtreeLayout = TerrainWorldGridLayer::MakeQuadtreeLayout(GetCellSize());

    if (!m_quadtreeLayout.IsValid())
    {
        HYP_LOG(WorldGrid, Error, "Cell {} has cell size {} - terrain LOD needs a cell size of 2^n + 1", m_cellInfo.coord, GetCellSize());
    }

    UpdateNodeHeightBounds();

    m_nodeInRange.Resize(m_quadtreeLayout.GetNumNodes());

    for (uint8& nodeInRange : m_nodeInRange)
    {
        nodeInRange = 0;
    }

    m_patches.Resize(m_quadtreeLayout.GetNumPatches());
}

void TerrainStreamingCell::UpdateNodeHeightBounds()
{
    ComputeTerrainQuadtreeNodeHeightBounds(m_quadtreeLayout, m_paddedHeights, GetCellSize(), m_nodeMinHeights, m_nodeMaxHeights);
}

BoundingBox TerrainStreamingCell::GetNodeWorldBounds(uint32 nodeIndex) const
{
    const TerrainQuadtreeLayout::NodeKey node = m_quadtreeLayout.GetNodeKey(nodeIndex);

    const Vec2u origin = m_quadtreeLayout.GetNodeOrigin(node);
    const float gridQuads = float(m_quadtreeLayout.GetNodeGridQuads(node.level));

    const Vec3f& tileMin = m_cellInfo.bounds.min;
    const Vec3f& scale = m_cellInfo.scale;

    return BoundingBox(
        Vec3f(tileMin.x + float(origin.x) * scale.x, tileMin.y + m_nodeMinHeights[nodeIndex], tileMin.z + float(origin.y) * scale.z),
        Vec3f(tileMin.x + (float(origin.x) + gridQuads) * scale.x, tileMin.y + m_nodeMaxHeights[nodeIndex], tileMin.z + (float(origin.y) + gridQuads) * scale.z));
}

BoundingBox TerrainStreamingCell::GetPatchWorldBounds(uint32 patchIndex) const
{
    const TerrainQuadtreeLayout::PatchKey patchKey = m_quadtreeLayout.GetPatchKey(patchIndex);

    // a quadrant patch covers exactly its child node's footprint
    const TerrainQuadtreeLayout::NodeKey footprintNode = m_quadtreeLayout.HasQuadrantChildren(patchKey.node.level)
        ? m_quadtreeLayout.GetChildNode(patchKey.node, patchKey.quadrant)
        : patchKey.node;

    return GetNodeWorldBounds(m_quadtreeLayout.GetNodeIndex(footprintNode));
}

void TerrainStreamingCell::ComputeNodeDistances(Span<const Vec3f> viewpoints, Array<float>& outNodeDistances) const
{
    const uint32 numNodes = m_quadtreeLayout.GetNumNodes();

    outNodeDistances.Resize(numNodes);

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
    {
        const BoundingBox nodeWorldBounds = GetNodeWorldBounds(nodeIndex);

        float nearestDistance = MathUtil::Infinity<float>();

        for (const Vec3f& viewpoint : viewpoints)
        {
            nearestDistance = MathUtil::Min(nearestDistance, DistanceToBounds(viewpoint, nodeWorldBounds));
        }

        outNodeDistances[nodeIndex] = nearestDistance;
    }
}

float TerrainStreamingCell::GetLodRange(uint8 level) const
{
    return TerrainWorldGridLayer::CalculateLodRange(level, m_quadtreeLayout, m_cellInfo.scale);
}

float TerrainStreamingCell::GetLodMorphStart(uint8 level) const
{
    return TerrainWorldGridLayer::CalculateLodMorphStart(level, m_quadtreeLayout, m_cellInfo.scale);
}

bool TerrainStreamingCell::IsNodeResident(uint32 nodeIndex) const
{
    const TerrainQuadtreeLayout::NodeKey node = m_quadtreeLayout.GetNodeKey(nodeIndex);

    for (uint32 quadrant = 0; quadrant < m_quadtreeLayout.GetPatchesPerNode(node.level); quadrant++)
    {
        if (!m_patches[m_quadtreeLayout.GetPatchIndex({ node, quadrant })].mesh.IsValid())
        {
            return false;
        }
    }

    return true;
}

bool TerrainStreamingCell::IsNodeWanted(uint32 nodeIndex, float nodeDistance) const
{
    constexpr float BuildRangeScale = 1.5f;
    constexpr float ReleaseRangeScale = 2.0f;

    const uint8 level = m_quadtreeLayout.GetNodeKey(nodeIndex).level;

    if (level == m_quadtreeLayout.GetTopLevel())
    {
        return true;
    }

    const float rangeScale = IsNodeResident(nodeIndex) ? ReleaseRangeScale : BuildRangeScale;

    return nodeDistance < GetLodRange(level) * rangeScale;
}

void TerrainStreamingCell::BuildInitialPatchMeshData()
{
    HYP_SCOPE;

    m_initialPatchBuilds.Clear();

    if (!m_quadtreeLayout.IsValid())
    {
        return;
    }

    const Array<Vec3f> viewpoints = m_layer->GetLodViewpoints();

    Array<float> nodeDistances;
    ComputeNodeDistances(viewpoints, nodeDistances);

    TerrainMeshBuilder meshBuilder(GetCellSize(), m_quadtreeLayout);

    for (uint32 nodeIndex = 0; nodeIndex < m_quadtreeLayout.GetNumNodes(); nodeIndex++)
    {
        if (!IsNodeWanted(nodeIndex, nodeDistances[nodeIndex]))
        {
            continue;
        }

        const TerrainQuadtreeLayout::NodeKey node = m_quadtreeLayout.GetNodeKey(nodeIndex);

        for (uint32 quadrant = 0; quadrant < m_quadtreeLayout.GetPatchesPerNode(node.level); quadrant++)
        {
            const uint32 patchIndex = m_quadtreeLayout.GetPatchIndex({ node, quadrant });

            m_initialPatchBuilds.PushBack(TerrainPatchBuild { patchIndex, meshBuilder.BuildPatchMeshData(m_paddedHeights, patchIndex) });
        }
    }
}

void TerrainStreamingCell::UpdateLodSelection(Span<const Vec3f> viewpoints)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_isRemoved || !m_node.IsValid() || !m_quadtreeLayout.IsValid())
    {
        return;
    }

    const uint32 numNodes = m_quadtreeLayout.GetNumNodes();
    const uint32 numPatches = m_quadtreeLayout.GetNumPatches();

    Array<float> nodeDistances;
    ComputeNodeDistances(viewpoints, nodeDistances);

    Array<uint8> nodeResident;
    nodeResident.Resize(numNodes);

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
    {
        nodeResident[nodeIndex] = IsNodeResident(nodeIndex) ? 1 : 0;
    }

    Array<float> levelRanges;
    levelRanges.Resize(m_quadtreeLayout.GetNumLevels());

    for (uint8 level = 0; level < m_quadtreeLayout.GetNumLevels(); level++)
    {
        levelRanges[level] = GetLodRange(level);
    }

    Array<uint8> nodeInRange;
    nodeInRange.Resize(numNodes);

    Array<uint8> patchDrawn;
    patchDrawn.Resize(numPatches);

    const TerrainQuadtreeSelectionInput selectionInput {
        m_quadtreeLayout,
        nodeDistances,
        levelRanges,
        nodeResident,
        m_nodeInRange
    };

    SelectTerrainQuadtreePatches(selectionInput, nodeInRange, patchDrawn);

    m_nodeInRange = std::move(nodeInRange);

    UpdateGrassSelection(viewpoints);

    for (uint32 patchIndex = 0; patchIndex < numPatches; patchIndex++)
    {
        TerrainPatch& patch = m_patches[patchIndex];

        patch.isDrawn = patchDrawn[patchIndex] != 0 && patch.mesh.IsValid();

        if (!patch.isDrawn)
        {
            continue;
        }

        // the shader measures the morph from here, and so does SampleDrawnHeight()
        const BoundingBox patchWorldBounds = GetPatchWorldBounds(patchIndex);

        float nearestDistance = MathUtil::Infinity<float>();
        patch.lodMorphOrigin = patchWorldBounds.GetCenter();

        for (const Vec3f& viewpoint : viewpoints)
        {
            const float distance = DistanceToBounds(viewpoint, patchWorldBounds);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                patch.lodMorphOrigin = viewpoint;
            }
        }
    }

    // no viewpoint (e.g. between camera switches) would otherwise release everything but the top node
    if (!viewpoints || IsStale())
    {
        return;
    }

    Array<uint32> patchesToBuild;
    Array<uint32> patchesToRelease;

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
    {
        const bool isWanted = IsNodeWanted(nodeIndex, nodeDistances[nodeIndex]);

        const TerrainQuadtreeLayout::NodeKey node = m_quadtreeLayout.GetNodeKey(nodeIndex);

        for (uint32 quadrant = 0; quadrant < m_quadtreeLayout.GetPatchesPerNode(node.level); quadrant++)
        {
            const uint32 patchIndex = m_quadtreeLayout.GetPatchIndex({ node, quadrant });
            const TerrainPatch& patch = m_patches[patchIndex];

            if (isWanted && !patch.mesh.IsValid() && !patch.isBuildQueued)
            {
                patchesToBuild.PushBack(patchIndex);
            }
            else if (!isWanted && patch.mesh.IsValid() && !patch.isDrawn)
            {
                patchesToRelease.PushBack(patchIndex);
            }
        }
    }

    if (patchesToBuild.Any())
    {
        QueuePatchBuilds(std::move(patchesToBuild));
    }

    if (patchesToRelease.Any())
    {
        // entities can't be removed while systems are processing
        if (ThreadBase* simThread = GetThreadById(g_simThread))
        {
            simThread->GetScheduler().Enqueue(
                [weakThis = WeakHandleFromThis(), patchesToRelease = std::move(patchesToRelease)]() mutable
                {
                    if (Handle<TerrainStreamingCell> cell = weakThis.Lock(); cell.IsValid())
                    {
                        cell->ReleaseUnwantedPatches(std::move(patchesToRelease));
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

bool TerrainStreamingCell::ApplyPatchLod(
    MeshComponent& meshComponent,
    TerrainPatchComponent& patchComponent,
    bool& outDrawnMeshChanged) const
{
    outDrawnMeshChanged = false;

    const uint32 patchIndex = patchComponent.patchIndex;
    const bool isDrawn = patchIndex < m_patches.Size() && m_patches[patchIndex].isDrawn;
    const Handle<Mesh> drawnMesh = isDrawn ? m_patches[patchIndex].mesh : Handle<Mesh>();

    if (meshComponent.mesh != drawnMesh)
    {
        meshComponent.mesh = drawnMesh;

        outDrawnMeshChanged = true;
    }

    if (!isDrawn)
    {
        return outDrawnMeshChanged;
    }

    const Vec3f& nearestViewpoint = m_patches[patchIndex].lodMorphOrigin;

    const float morphStart = GetLodMorphStart(patchComponent.level);
    const float morphEnd = GetLodRange(patchComponent.level);
    const float rangeMultiplier = TerrainWorldGridLayer::GetLodRangeMultiplier();

    // the origin moves with the camera even when nothing else changes, and the shader only sees it once the render
    // proxy is refreshed
    constexpr float OriginEpsilonSquared = 0.01f;

    const bool morphChanged = patchComponent.lodMorphStart != morphStart
        || patchComponent.lodMorphEnd != morphEnd
        || patchComponent.lodRangeMultiplier != rangeMultiplier
        || patchComponent.lodMorphOrigin.DistanceSquared(nearestViewpoint) > OriginEpsilonSquared;

    if (morphChanged)
    {
        patchComponent.lodMorphStart = morphStart;
        patchComponent.lodMorphEnd = morphEnd;
        patchComponent.lodRangeMultiplier = rangeMultiplier;
        patchComponent.lodMorphOrigin = nearestViewpoint;
    }

    return outDrawnMeshChanged || morphChanged;
}

void TerrainStreamingCell::QueuePatchBuilds(Array<uint32>&& patchIndices)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (uint32 patchIndex : patchIndices)
    {
        m_patches[patchIndex].isBuildQueued = true;
    }

    TaskSystem::GetInstance().Enqueue(
        [weakThis = WeakHandleFromThis(),
            paddedHeights = m_paddedHeights,
            cellSize = GetCellSize(),
            layout = m_quadtreeLayout,
            patchIndices = std::move(patchIndices),
            buildGeneration = m_patchBuildGeneration]()
        {
            TerrainMeshBuilder meshBuilder(cellSize, layout);

            Array<TerrainPatchBuild> patchBuilds;
            patchBuilds.Reserve(patchIndices.Size());

            for (uint32 patchIndex : patchIndices)
            {
                patchBuilds.PushBack(TerrainPatchBuild { patchIndex, meshBuilder.BuildPatchMeshData(paddedHeights, patchIndex) });
            }

            ThreadBase* simThread = GetThreadById(g_simThread);

            if (!simThread)
            {
                return;
            }

            simThread->GetScheduler().Enqueue(
                [weakThis, patchBuilds = std::move(patchBuilds), buildGeneration]() mutable
                {
                    if (Handle<TerrainStreamingCell> cell = weakThis.Lock(); cell.IsValid())
                    {
                        cell->ApplyPatchBuilds(std::move(patchBuilds), buildGeneration);
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void TerrainStreamingCell::ApplyPatchBuilds(Array<TerrainPatchBuild>&& patchBuilds, uint32 buildGeneration)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    // superseded by a brush edit, or the cell went away while building
    const bool isCurrent = buildGeneration == m_patchBuildGeneration && !m_isRemoved && m_node.IsValid() && !IsStale();

    for (const TerrainPatchBuild& patchBuild : patchBuilds)
    {
        if (patchBuild.patchIndex >= m_patches.Size())
        {
            continue;
        }

        // cleared either way, so a dropped build is requested again from the current heights
        m_patches[patchBuild.patchIndex].isBuildQueued = false;

        if (isCurrent)
        {
            CreatePatch(patchBuild.patchIndex, patchBuild.meshData);
        }
    }
}

void TerrainStreamingCell::ReleaseUnwantedPatches(Array<uint32>&& patchIndices)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_isRemoved || !m_node.IsValid())
    {
        return;
    }

    bool anyReleased = false;

    for (uint32 patchIndex : patchIndices)
    {
        if (patchIndex >= m_patches.Size() || !m_patches[patchIndex].mesh.IsValid() || m_patches[patchIndex].isDrawn)
        {
            continue;
        }

        ReleasePatch(patchIndex);

        anyReleased = true;
    }

    if (anyReleased)
    {
        // static shadow views that skipped collection still hold the released meshes
        m_scene->MarkStaticRenderResourcesChanged();
    }
}

void TerrainStreamingCell::CreatePatch(uint32 patchIndex, const TerrainPatchMeshData& meshData)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid() || !m_node.IsValid() || patchIndex >= m_patches.Size() || meshData.vertices.Empty())
    {
        return;
    }

    TerrainPatch& patch = m_patches[patchIndex];

    if (patch.mesh.IsValid())
    {
        return;
    }

    MeshDesc meshDesc;
    MeshDataView meshDataView {};
    BuildPatchMeshDescAndDataView(meshData, meshDesc, meshDataView);

    patch.mesh = MakeHandle<Mesh>();
    patch.mesh->SetName(NAME_FMT("TerrainPatchMesh_{}_{}", m_cellInfo.coord, patchIndex));
    patch.mesh->SetMeshData(meshDesc, meshDataView);
    patch.mesh->SetIsTransient(true);
    patch.mesh->SetIsDynamicMesh(true);
    InitObject(patch.mesh);

    const Transform transform = ComputeTileTransform();

    EntityInitInfo entityInitInfo {};
    entityInitInfo.bvhDepth = 0; // don't build bvhs to save load times

    patch.entity = MakeHandle<Entity>(NAME_FMT("TerrainPatch_{}_{}", m_cellInfo.coord, patchIndex), entityInitInfo);
    patch.entity->SetLocalBounds(patch.mesh->GetAABB());
    patch.entity->SetIsStatic(true);

    entityManager->AddExistingEntity(patch.entity);

    entityManager->GetComponent<TransformComponent>(patch.entity) = TransformComponent {
        transform.GetTranslation(),
        transform.GetRotation(),
        transform.GetScale()
    };

    entityManager->GetComponent<VisibilityStateComponent>(patch.entity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

    // added with its mesh so the component validates, then hidden until the LOD selection draws it
    MeshComponent& meshComponent = entityManager->AddComponent<MeshComponent>(patch.entity, MeshComponent { patch.mesh, GetTileMaterial() });
    meshComponent.mesh = Handle<Mesh>();

    entityManager->AddComponent<TerrainPatchComponent>(patch.entity, TerrainPatchComponent {
        .cell = WeakHandleFromThis(),
        .patchIndex = patchIndex,
        .level = m_quadtreeLayout.GetPatchKey(patchIndex).node.level
    });

    m_node->AddChild(patch.entity);
}

void TerrainStreamingCell::ReleasePatch(uint32 patchIndex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    TerrainPatch& patch = m_patches[patchIndex];

    if (patch.entity.IsValid())
    {
        patch.entity->Remove(/* moveToDetached */ false);
    }

    const bool isBuildQueued = patch.isBuildQueued;

    patch = TerrainPatch {};
    patch.isBuildQueued = isBuildQueued;
}

bool TerrainStreamingCell::WorldToGridPosition(const Vec2f& worldXZ, Vec2f& outGridXZ) const
{
    if (!m_quadtreeLayout.IsValid())
    {
        return false;
    }

    const Vec3f& scale = m_cellInfo.scale;

    if (scale.x <= 0.0f || scale.z <= 0.0f)
    {
        return false;
    }

    const Vec3f& tileMin = m_cellInfo.bounds.min;
    const float tileQuads = float(m_quadtreeLayout.GetTileQuads());

    outGridXZ = Vec2f((worldXZ.x - tileMin.x) / scale.x, (worldXZ.y - tileMin.z) / scale.z);

    return outGridXZ.x >= 0.0f && outGridXZ.x <= tileQuads
        && outGridXZ.y >= 0.0f && outGridXZ.y <= tileQuads;
}

bool TerrainStreamingCell::FindDrawnPatchAt(const Vec2f& gridXZ, uint32& outPatchIndex) const
{
    if (m_patches.Size() != m_quadtreeLayout.GetNumPatches())
    {
        return false;
    }

    // the finest level wins: a coarser node's patches are only drawn where none of its children are
    for (uint8 level = 0; level < m_quadtreeLayout.GetNumLevels(); level++)
    {
        const uint32 nodeGridQuads = m_quadtreeLayout.GetNodeGridQuads(level);
        const uint32 nodesPerSide = m_quadtreeLayout.GetNodesPerSide(level);

        const TerrainQuadtreeLayout::NodeKey node {
            level,
            MathUtil::Min(uint32(gridXZ.x) / nodeGridQuads, nodesPerSide - 1),
            MathUtil::Min(uint32(gridXZ.y) / nodeGridQuads, nodesPerSide - 1)
        };

        uint32 quadrant = 0;

        if (m_quadtreeLayout.HasQuadrantChildren(level))
        {
            const Vec2u nodeOrigin = m_quadtreeLayout.GetNodeOrigin(node);
            const uint32 halfGridQuads = nodeGridQuads / 2;

            const uint32 quadrantX = MathUtil::Min(uint32(gridXZ.x - float(nodeOrigin.x)) / halfGridQuads, 1u);
            const uint32 quadrantZ = MathUtil::Min(uint32(gridXZ.y - float(nodeOrigin.y)) / halfGridQuads, 1u);

            quadrant = quadrantZ * 2 + quadrantX;
        }

        const uint32 patchIndex = m_quadtreeLayout.GetPatchIndex({ node, quadrant });

        if (m_patches[patchIndex].isDrawn)
        {
            outPatchIndex = patchIndex;

            return true;
        }
    }

    return false;
}

bool TerrainStreamingCell::SampleSurfaceHeight(const Vec2f& worldXZ, float& outHeight) const
{
    Vec2f gridXZ;

    if (!WorldToGridPosition(worldXZ, gridXZ) || m_paddedHeights.Empty())
    {
        return false;
    }

    const float height = TerrainMeshHelpers::SampleLodSurfaceHeight(
        m_paddedHeights.ToSpan(),
        GetCellSize(),
        /* stride */ 1,
        gridXZ.x,
        gridXZ.y);

    outHeight = m_cellInfo.bounds.min.y + height * m_cellInfo.scale.y;

    return true;
}

bool TerrainStreamingCell::SampleDrawnHeight(const Vec2f& worldXZ, float& outHeight) const
{
    Vec2f gridXZ;
    uint32 patchIndex;

    if (!WorldToGridPosition(worldXZ, gridXZ) || m_paddedHeights.Empty() || !FindDrawnPatchAt(gridXZ, patchIndex))
    {
        return false;
    }

    const TerrainPatch& patch = m_patches[patchIndex];

    const uint8 level = m_quadtreeLayout.GetPatchKey(patchIndex).node.level;
    const uint8 topLevel = m_quadtreeLayout.GetTopLevel();

    const uint32 cellSize = GetCellSize();
    const Span<const float> paddedHeights = m_paddedHeights.ToSpan();

    const auto lodSurfaceHeightAt = [&](uint8 lodLevel) -> float
    {
        return TerrainMeshHelpers::SampleLodSurfaceHeight(
            paddedHeights,
            cellSize,
            TerrainQuadtreeLayout::GetStride(MathUtil::Min<uint8>(lodLevel, topLevel)),
            gridXZ.x,
            gridXZ.y);
    };

    float height = lodSurfaceHeightAt(level);

    // mirrors ApplyTerrainMorph() in TerrainMorph.hlsli - the morph is a function of the unmorphed world position
    const float morphStart = GetLodMorphStart(level);
    const float morphEnd = GetLodRange(level);

    if (morphEnd > morphStart)
    {
        const Vec3f worldPosition(worldXZ.x, m_cellInfo.bounds.min.y + height * m_cellInfo.scale.y, worldXZ.y);
        const float morphDistance = patch.lodMorphOrigin.Distance(worldPosition);

        const float rangeMultiplier = TerrainWorldGridLayer::GetLodRangeMultiplier();

        const float nextLodMorph = MathUtil::Clamp((morphDistance - morphStart) / (morphEnd - morphStart), 0.0f, 1.0f);
        const float secondLodMorph = MathUtil::Clamp((morphDistance - morphStart * rangeMultiplier) / ((morphEnd - morphStart) * rangeMultiplier), 0.0f, 1.0f);

        height = MathUtil::Lerp(
            MathUtil::Lerp(height, lodSurfaceHeightAt(uint8(level + 1)), nextLodMorph),
            lodSurfaceHeightAt(uint8(level + 2)),
            secondLodMorph);
    }

    outHeight = m_cellInfo.bounds.min.y + height * m_cellInfo.scale.y;

    return true;
}

void TerrainStreamingCell::RebuildPatchesInRegion(const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_quadtreeLayout.IsValid())
    {
        return;
    }

    UpdateNodeHeightBounds();

    // builds still in flight read the previous heights
    m_patchBuildGeneration++;

    TerrainMeshBuilder meshBuilder(GetCellSize(), m_quadtreeLayout);

    const int32 tileQuads = int32(m_quadtreeLayout.GetTileQuads());

    // a patch only reads heights on its own grid lines - the coarser grids its morph targets sample sit on a subset of
    // them - plus the immediate neighbours each vertex takes its normal from. A brush that misses every one of those
    // lines cannot change the patch, so a coarse patch covering the whole tile is left alone by a small stroke
    const auto readsChangedGridLines = [](uint32 stride, int32 readMin, int32 readMax, int32 changedMin, int32 changedMax) -> bool
    {
        const int32 overlapMin = MathUtil::Max(readMin, changedMin - 1);
        const int32 overlapMax = MathUtil::Min(readMax, changedMax + 1);

        if (overlapMin > overlapMax)
        {
            return false;
        }

        // the highest grid line at or below overlapMax, which is inside the overlap if it reaches overlapMin
        return (overlapMax / int32(stride)) * int32(stride) >= overlapMin;
    };

    bool anyRebuilt = false;

    for (uint32 patchIndex = 0; patchIndex < uint32(m_patches.Size()); patchIndex++)
    {
        TerrainPatch& patch = m_patches[patchIndex];

        if (!patch.mesh.IsValid())
        {
            continue;
        }

        const TerrainQuadtreeLayout::PatchRegion region = m_quadtreeLayout.GetPatchRegion(m_quadtreeLayout.GetPatchKey(patchIndex));

        // morph targets sample the two coarser grids, up to four strides away from each vertex
        const int32 margin = int32(region.stride) * 4;

        const int32 readMinX = MathUtil::Max(int32(region.origin.x) - margin, 0);
        const int32 readMinZ = MathUtil::Max(int32(region.origin.y) - margin, 0);
        const int32 readMaxX = MathUtil::Min(int32(region.origin.x + region.gridQuads) + margin, tileQuads);
        const int32 readMaxZ = MathUtil::Min(int32(region.origin.y + region.gridQuads) + margin, tileQuads);

        if (!readsChangedGridLines(region.stride, readMinX, readMaxX, minVertex.x, maxVertex.x)
            || !readsChangedGridLines(region.stride, readMinZ, readMaxZ, minVertex.y, maxVertex.y))
        {
            continue;
        }

        const TerrainPatchMeshData patchMeshData = meshBuilder.BuildPatchMeshData(m_paddedHeights, patchIndex);

        if (patchMeshData.vertices.Empty())
        {
            // heights of an unexpected size - leave the patch drawing what it has rather than emptying its mesh
            continue;
        }

        if (patchMeshData.vertices.Size() == patch.mesh->GetMeshDesc().lods[0].numVertices)
        {
            VertexArrayView vertexRange {};
            vertexRange.floatData = reinterpret_cast<const float*>(patchMeshData.vertices.Data());
            vertexRange.vertexCount = patchMeshData.vertices.Size();
            vertexRange.layoutDesc = StaticVertexInputLayout<VT_Simple | VT_UV1>;

            patch.mesh->UpdateDynamicVertexData(0, 0, vertexRange);
        }
        else
        {
            MeshDesc meshDesc;
            MeshDataView meshDataView {};
            BuildPatchMeshDescAndDataView(patchMeshData, meshDesc, meshDataView);

            patch.mesh->SetMeshData(meshDesc, meshDataView);
            patch.mesh->UploadGpuData();
        }

        if (patch.entity.IsValid())
        {
            patch.entity->SetLocalBounds(patch.mesh->GetAABB());
            patch.entity->SetNeedsRenderProxyUpdate();
        }

        anyRebuilt = true;
    }

    if (anyRebuilt)
    {
        m_scene->MarkStaticRenderResourcesChanged();
    }
}

#pragma endregion Quadtree

#pragma region Grass

uint32 TerrainStreamingCell::GetNumGrassTilesPerSide() const
{
    const uint32 cellQuads = GetCellSize() > 1 ? GetCellSize() - 1 : 0;

    return (cellQuads + TerrainGrassTileQuads - 1) / TerrainGrassTileQuads;
}

void TerrainStreamingCell::SetSplatWeights(const Array<ubyte>& splatBytes, bool rowsFlipped)
{
    const uint32 cellSize = GetCellSize();
    const size_t rowSize = size_t(cellSize) * TerrainNumSplatLayers;

    if (splatBytes.Size() < rowSize * cellSize)
    {
        m_splatWeights = Array<ubyte>();

        return;
    }

    m_splatWeights.Resize(rowSize * cellSize);

    for (uint32 z = 0; z < cellSize; z++)
    {
        const uint32 sourceRow = rowsFlipped ? cellSize - 1 - z : z;

        Memory::Copy(m_splatWeights.Data() + z * rowSize, splatBytes.Data() + sourceRow * rowSize, rowSize);
    }

    m_grassSplatWeightsSnapshot.Reset();
}

void TerrainStreamingCell::SetGroundCoverPaint(const TerrainCellData& cellData)
{
    const size_t planeSize = size_t(GetCellSize()) * GetCellSize();
    const Array<Name>& paintLayers = cellData.GetGroundCoverPaintLayers();
    const ConstByteView paint = cellData.GetGroundCoverPaint();

    m_grassPaintSnapshot.Reset();

    if (paint.Size() != planeSize * paintLayers.Size())
    {
        if (paint.Size() != 0)
        {
            HYP_LOG(WorldGrid, Warning, "Ground cover paint for cell {} is {} bytes but {} layers need {} - ignoring it",
                m_cellInfo.coord, paint.Size(), paintLayers.Size(), planeSize * paintLayers.Size());
        }

        m_groundCoverPaint = Array<ubyte>();
        m_groundCoverPaintLayers = Array<Name>();

        return;
    }

    m_groundCoverPaint.Resize(paint.Size());
    Memory::Copy(m_groundCoverPaint.Data(), paint.Data(), paint.Size());

    m_groundCoverPaintLayers = paintLayers;
}

void TerrainStreamingCell::InvalidateGrass(const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 tilesPerSide = GetNumGrassTilesPerSide();

    if (m_grassTiles.Size() != size_t(tilesPerSide) * tilesPerSide)
    {
        return;
    }

    const int32 tileQuads = int32(TerrainGrassTileQuads);

    for (uint32 tileZ = 0; tileZ < tilesPerSide; tileZ++)
    {
        for (uint32 tileX = 0; tileX < tilesPerSide; tileX++)
        {
            const int32 tileMinX = int32(tileX) * tileQuads;
            const int32 tileMinZ = int32(tileZ) * tileQuads;

            if (maxVertex.x < tileMinX - 1 || minVertex.x > tileMinX + tileQuads + 1
                || maxVertex.y < tileMinZ - 1 || minVertex.y > tileMinZ + tileQuads + 1)
            {
                continue;
            }

            GrassTile& tile = m_grassTiles[tileZ * tilesPerSide + tileX];

            tile.isBuilt = false;
            tile.buildGeneration++;
        }
    }
}

void TerrainStreamingCell::UpdateGrassSelection(Span<const Vec3f> viewpoints)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_isRemoved || !m_node.IsValid() || !m_layer.IsValid() || EngineGlobals::IsHeadless())
    {
        return;
    }

    const uint32 tilesPerSide = GetNumGrassTilesPerSide();
    const size_t numTiles = size_t(tilesPerSide) * tilesPerSide;

    if (m_grassTiles.Size() != numTiles)
    {
        ReleaseAllGrass();

        m_grassTiles.Resize(numTiles);
    }

    const uint32 coverVersion = m_layer->GetGroundCoverResources().GetVersion();

    const bool isEnabled = g_cvTerrainGrass.Get() && m_splatWeights.Any() && viewpoints.Size() != 0;

    const float plantRange = MathUtil::Max(g_cvTerrainGrassRange.Get(), 0.0f);

    // released a tile's width further out than planted, so a viewpoint on a tile boundary doesn't churn
    const float releaseRange = plantRange + float(TerrainGrassTileQuads) * MathUtil::Max(m_cellInfo.scale.x, m_cellInfo.scale.z);

    const BoundingBox& cellBounds = m_cellInfo.bounds;
    const uint32 cellQuads = GetCellSize() - 1;

    const float nearRange = MathUtil::Max(g_cvTerrainGrassNearRange.Get(), 0.0f);
    const float nearHysteresis = float(TerrainGrassTileQuads) * 0.5f * MathUtil::Max(m_cellInfo.scale.x, m_cellInfo.scale.z);

    Array<uint32> tilesToBuild;
    Array<uint8> tileDetailLevels;
    Array<uint32> tilesToRelease;

    for (uint32 tileIndex = 0; tileIndex < uint32(numTiles); tileIndex++)
    {
        GrassTile& tile = m_grassTiles[tileIndex];

        float nearestDistance = MathUtil::MaxSafeValue<float>();

        if (isEnabled)
        {
            const uint32 tileX = tileIndex % tilesPerSide;
            const uint32 tileZ = tileIndex / tilesPerSide;

            const float minX = float(tileX * TerrainGrassTileQuads);
            const float minZ = float(tileZ * TerrainGrassTileQuads);
            const float maxX = float(MathUtil::Min((tileX + 1) * TerrainGrassTileQuads, cellQuads));
            const float maxZ = float(MathUtil::Min((tileZ + 1) * TerrainGrassTileQuads, cellQuads));

            const BoundingBox tileBounds(
                Vec3f(cellBounds.min.x + minX * m_cellInfo.scale.x, cellBounds.min.y, cellBounds.min.z + minZ * m_cellInfo.scale.z),
                Vec3f(cellBounds.min.x + maxX * m_cellInfo.scale.x, cellBounds.max.y, cellBounds.min.z + maxZ * m_cellInfo.scale.z));

            for (const Vec3f& viewpoint : viewpoints)
            {
                nearestDistance = MathUtil::Min(nearestDistance, DistanceToBounds(viewpoint, tileBounds));
            }
        }

        if (nearestDistance <= plantRange)
        {
            uint8 detailLevel = tile.detailLevel;

            if (nearestDistance <= nearRange)
            {
                detailLevel = 0;
            }
            else if (nearestDistance > nearRange + nearHysteresis)
            {
                detailLevel = 1;
            }

            // a tile being replanted keeps its old cover until the replacement is built
            if (!tile.isBuildQueued && (!tile.isBuilt || detailLevel != tile.detailLevel || tile.coverVersion != coverVersion))
            {
                tilesToBuild.PushBack(tileIndex);
                tileDetailLevels.PushBack(detailLevel);
            }
        }
        else if (nearestDistance > releaseRange && (tile.isBuilt || tile.slots.Any()))
        {
            tilesToRelease.PushBack(tileIndex);
        }
    }

    if (tilesToBuild.Any())
    {
        QueueGrassBuilds(std::move(tilesToBuild), std::move(tileDetailLevels));
    }

    if (tilesToRelease.Any())
    {
        for (uint32 tileIndex : tilesToRelease)
        {
            // stops it being replanted before the deferred release runs
            m_grassTiles[tileIndex].isBuilt = false;
        }

        // entities can't be removed while systems are processing
        if (ThreadBase* simThread = GetThreadById(g_simThread))
        {
            simThread->GetScheduler().Enqueue(
                [weakThis = WeakHandleFromThis(), tilesToRelease = std::move(tilesToRelease)]() mutable
                {
                    if (Handle<TerrainStreamingCell> cell = weakThis.Lock(); cell.IsValid())
                    {
                        cell->ReleaseUnwantedGrassTiles(std::move(tilesToRelease));
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void TerrainStreamingCell::QueueGrassBuilds(Array<uint32>&& tileIndices, Array<uint8>&& detailLevels)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Array<uint32> tileBuildGenerations;
    tileBuildGenerations.Reserve(tileIndices.Size());

    for (uint32 tileIndex : tileIndices)
    {
        m_grassTiles[tileIndex].isBuildQueued = true;

        tileBuildGenerations.PushBack(m_grassTiles[tileIndex].buildGeneration);
    }

    if (!m_grassHeightsSnapshot.IsValid())
    {
        m_grassHeightsSnapshot = MakeShared<Array<float>>(m_paddedHeights);
    }

    if (!m_grassSplatWeightsSnapshot.IsValid())
    {
        m_grassSplatWeightsSnapshot = MakeShared<Array<ubyte>>(m_splatWeights);
    }

    if (!m_grassPaintSnapshot.IsValid())
    {
        m_grassPaintSnapshot = MakeShared<Array<ubyte>>(m_groundCoverPaint);
    }

    TerrainGroundCoverResources& coverResources = m_layer->GetGroundCoverResources();

    Array<TerrainCoverLayerPlan> layerPlans = coverResources.GetPlans();

    const float densityScale = MathUtil::Max(g_cvTerrainGrassDensity.Get(), 0.01f);

    Array<int32> layerPaintPlanes;
    layerPaintPlanes.Reserve(layerPlans.Size());

    for (TerrainCoverLayerPlan& layerPlan : layerPlans)
    {
        layerPlan.spacing /= MathUtil::Sqrt(densityScale);

        int32 paintPlane = -1;

        if (layerPlan.isPainted)
        {
            for (uint32 planeIndex = 0; planeIndex < uint32(m_groundCoverPaintLayers.Size()); planeIndex++)
            {
                if (m_groundCoverPaintLayers[planeIndex] == layerPlan.name)
                {
                    paintPlane = int32(planeIndex);

                    break;
                }
            }
        }

        layerPaintPlanes.PushBack(paintPlane);
    }

    TaskSystem::GetInstance().Enqueue(
        [weakThis = WeakHandleFromThis(),
            paddedHeights = m_grassHeightsSnapshot,
            splatWeights = m_grassSplatWeightsSnapshot,
            paintWeights = m_grassPaintSnapshot,
            layerPaintPlanes = std::move(layerPaintPlanes),
            cellSize = GetCellSize(),
            cellMin = m_cellInfo.bounds.min,
            cellScale = m_cellInfo.scale,
            coord = m_cellInfo.coord,
            tilesPerSide = GetNumGrassTilesPerSide(),
            layerPlans = std::move(layerPlans),
            coverVersion = coverResources.GetVersion(),
            tileIndices = std::move(tileIndices),
            detailLevels = std::move(detailLevels),
            tileBuildGenerations = std::move(tileBuildGenerations),
            buildGeneration = m_grassBuildGeneration]()
        {
            Array<GrassTileBuild> tileBuilds;
            tileBuilds.Reserve(tileIndices.Size());

            const uint32 cellQuads = cellSize - 1;

            for (uint32 buildIndex = 0; buildIndex < uint32(tileIndices.Size()); buildIndex++)
            {
                const uint32 tileIndex = tileIndices[buildIndex];
                const uint8 detailLevel = detailLevels[buildIndex];

                const uint32 tileX = tileIndex % tilesPerSide;
                const uint32 tileZ = tileIndex / tilesPerSide;

                TerrainGrassTileInput input;
                input.paddedHeights = paddedHeights->ToSpan();
                input.splatWeights = splatWeights->ToSpan();
                input.paintWeights = paintWeights->ToSpan();
                input.layerPaintPlanes = layerPaintPlanes.ToSpan();
                input.cellSize = cellSize;
                input.cellMin = cellMin;
                input.cellScale = cellScale;
                input.tileMin = Vec2u(tileX * TerrainGrassTileQuads, tileZ * TerrainGrassTileQuads);
                input.tileMax = Vec2u(
                    MathUtil::Min((tileX + 1) * TerrainGrassTileQuads, cellQuads),
                    MathUtil::Min((tileZ + 1) * TerrainGrassTileQuads, cellQuads));
                input.seed = uint32(HashCode::GetHashCode(coord.x).Combine(HashCode::GetHashCode(coord.y)).Combine(HashCode::GetHashCode(tileIndex)).Value());
                input.stretch = detailLevel == 0 ? 1.0f : s_grassFarStretch;
                input.layers = layerPlans.ToSpan();

                GrassTileBuild tileBuild;
                tileBuild.tileIndex = tileIndex;
                tileBuild.detailLevel = detailLevel;
                tileBuild.coverVersion = coverVersion;
                tileBuild.tileBuildGeneration = tileBuildGenerations[buildIndex];

                TerrainGrass::GenerateTile(input, tileBuild.output);

                tileBuilds.PushBack(std::move(tileBuild));
            }

            ThreadBase* simThread = GetThreadById(g_simThread);

            if (!simThread)
            {
                return;
            }

            simThread->GetScheduler().Enqueue(
                [weakThis, tileBuilds = std::move(tileBuilds), buildGeneration]() mutable
                {
                    if (Handle<TerrainStreamingCell> cell = weakThis.Lock(); cell.IsValid())
                    {
                        cell->ApplyGrassBuilds(std::move(tileBuilds), buildGeneration);
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void TerrainStreamingCell::ApplyGrassBuilds(Array<GrassTileBuild>&& tileBuilds, uint32 buildGeneration)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const bool isCurrent = buildGeneration == m_grassBuildGeneration && !m_isRemoved && m_node.IsValid() && m_layer.IsValid() && !IsStale();

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!isCurrent || !entityManager.IsValid())
    {
        for (const GrassTileBuild& tileBuild : tileBuilds)
        {
            if (tileBuild.tileIndex < m_grassTiles.Size())
            {
                m_grassTiles[tileBuild.tileIndex].isBuildQueued = false;
            }
        }

        return;
    }

    TerrainGroundCoverResources& coverResources = m_layer->GetGroundCoverResources();

    const uint32 coverVersion = coverResources.GetVersion();
    const Array<TerrainCoverLayer>& coverLayers = coverResources.GetLayers();
    const uint32 numSlots = coverResources.GetNumSlots();

    bool anyChanged = false;

    for (GrassTileBuild& tileBuild : tileBuilds)
    {
        if (tileBuild.tileIndex >= m_grassTiles.Size())
        {
            continue;
        }

        GrassTile& tile = m_grassTiles[tileBuild.tileIndex];

        tile.isBuildQueued = false;

        // planted from ground cover that has since changed - asked for again from the current one
        if (tileBuild.coverVersion != coverVersion
            || tileBuild.tileBuildGeneration != tile.buildGeneration
            || tileBuild.output.slots.Size() != numSlots)
        {
            continue;
        }

        if (tile.slots.Size() != numSlots || tile.coverVersion != coverVersion)
        {
            for (GrassTileSlot& slot : tile.slots)
            {
                ReleaseGrassTileSlot(slot);
            }

            tile.slots.Clear();
            tile.slots.Resize(numSlots);
        }

        tile.isBuilt = true;
        tile.detailLevel = tileBuild.detailLevel;
        tile.coverVersion = coverVersion;

        for (const TerrainCoverLayer& coverLayer : coverLayers)
        {
            for (const TerrainCoverType& coverType : coverLayer.types)
            {
                for (uint32 memberIndex = 0; memberIndex < uint32(coverType.members.Size()); memberIndex++)
                {
                    const uint32 slotIndex = coverType.firstSlot + memberIndex;

                    const TerrainCoverMember& member = coverType.members[memberIndex];
                    const TerrainGrassSlotInstances& instances = tileBuild.output.slots[slotIndex];

                    GrassTileSlot& slot = tile.slots[slotIndex];

                    const uint32 numInstances = uint32(instances.transforms.Size());

                    anyChanged = true;

                    if (numInstances == 0)
                    {
                        ReleaseGrassTileSlot(slot);

                        continue;
                    }

                    // far tiles' shadows are a few texels of the cascade they land in - not worth drawing every patch into it
                    const Handle<Material>& material = tileBuild.detailLevel == 0 ? member.material : member.materialNoShadows;

                    if (!slot.instanceData.IsValid())
                    {
                        slot.instanceData = MakeHandle<InstancedMeshData>(NAME_FMT("TerrainGrass_{}_{}_{}", m_cellInfo.coord, tileBuild.tileIndex, slotIndex));
                        slot.instanceData->SetIsTransient(true);
                        InitObject(slot.instanceData);
                    }

                    {
                        auto writeScope = slot.instanceData->GetWriteScope();

                        slot.instanceData->SetBufferData(0, instances.transforms.Data(), numInstances);
                        slot.instanceData->SetBufferData(1, instances.transforms.Data(), numInstances);
                    }

                    if (!slot.entity.IsValid())
                    {
                        EntityInitInfo entityInitInfo {};
                        entityInitInfo.bvhDepth = 0;

                        slot.entity = MakeHandle<Entity>(NAME_FMT("TerrainGrass_{}_{}_{}", m_cellInfo.coord, tileBuild.tileIndex, slotIndex), entityInitInfo);
                        slot.entity->SetIsStatic(true);

                        // not part of the node hierarchy, so it's never saved or listed, but it draws and culls like any entity of the scene
                        entityManager->AddExistingEntity(slot.entity);

                        MeshComponent meshComponent { member.mesh, material };
                        meshComponent.instanceData = AssetReference(Handle<AssetObject>(slot.instanceData));
                        meshComponent.numInstances = numInstances;

                        entityManager->AddComponent<MeshComponent>(slot.entity, std::move(meshComponent));
                    }
                    else if (MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(slot.entity))
                    {
                        meshComponent->numInstances = numInstances;
                        meshComponent->material = material;
                    }

                    slot.entity->SetLocalBounds(instances.bounds);
                    slot.entity->SetNeedsRenderProxyUpdate();
                }
            }
        }
    }

    if (anyChanged)
    {
        m_scene->MarkStaticRenderResourcesChanged();
    }
}

void TerrainStreamingCell::ReleaseUnwantedGrassTiles(Array<uint32>&& tileIndices)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_isRemoved || !m_node.IsValid())
    {
        return;
    }

    bool anyReleased = false;

    for (uint32 tileIndex : tileIndices)
    {
        if (tileIndex >= m_grassTiles.Size() || m_grassTiles[tileIndex].isBuilt)
        {
            continue;
        }

        ReleaseGrassTile(tileIndex);

        anyReleased = true;
    }

    if (anyReleased)
    {
        m_scene->MarkStaticRenderResourcesChanged();
    }
}

void TerrainStreamingCell::ReleaseGrassTileSlot(GrassTileSlot& slot)
{
    if (slot.entity.IsValid() && slot.entity->GetEntityManager() != nullptr)
    {
        slot.entity->Remove(/* moveToDetached */ false);
    }

    slot = GrassTileSlot {};
}

void TerrainStreamingCell::ReleaseGrassTile(uint32 tileIndex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    GrassTile& tile = m_grassTiles[tileIndex];

    for (GrassTileSlot& slot : tile.slots)
    {
        ReleaseGrassTileSlot(slot);
    }

    const bool isBuildQueued = tile.isBuildQueued;
    const uint32 buildGeneration = tile.buildGeneration;

    tile = GrassTile {};
    tile.isBuildQueued = isBuildQueued;
    tile.buildGeneration = buildGeneration + 1;
}

void TerrainStreamingCell::ReleaseAllGrass()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (uint32 tileIndex = 0; tileIndex < uint32(m_grassTiles.Size()); tileIndex++)
    {
        ReleaseGrassTile(tileIndex);
    }

    m_grassBuildGeneration++;
}

#pragma endregion Grass

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
