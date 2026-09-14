/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

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
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Vertex.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Guarded.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>
#endif

#include <TerrainStreamingCell.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#ifdef HYP_EDITOR

struct TerrainGenerationEditorTaskState
{
    ///cells queued for generation that haven't finished loading yet
    AtomicVar<int32> numPendingCells { 0 };
    Guarded<Handle<TickableEditorTask>> editorTask;

    void OnGenerationQueued()
    {
        numPendingCells.Increment(1, MemoryOrder::RELAXED);

        Update();
    }

    void OnGenerationFinished()
    {
        numPendingCells.Decrement(1, MemoryOrder::RELAXED);

        Update();
    }

    void Update()
    {
        auto readCount = [numPendingCells = &numPendingCells]() -> int
        {
            return numPendingCells->Get(MemoryOrder::RELAXED);
        };

        auto updateTaskWithCount = [](Handle<TickableEditorTask>& task, int count) -> bool
        {
            if (task.IsValid() && task->IsCancellationRequested())
            {
                task.Reset();

                return true;
            }

            if (count <= 0)
            {
                if (task.IsValid())
                {
                    task->Cancel();

                    if (task->IsCancellationRequested())
                    {
                        task.Reset();
                    }
                }

                return true;
            }

            if (task.IsValid())
            {
                task->SetDescription(HYP_FORMAT("{} cells", count));

                return true;
            }

            return false;
        };

        auto updateTask = [readCount, updateTaskWithCount](Handle<TickableEditorTask>& task, int& outCount) -> bool
        {
            outCount = readCount();

            return updateTaskWithCount(task, outCount);
        };

        editorTask.Access([this, readCount, updateTaskWithCount](Handle<TickableEditorTask>& task)
        {
            const int count = readCount();
            if (updateTaskWithCount(task, count))
            {
                return;
            }

            if (!g_editorState.IsValid() || !EngineGlobals::IsEditor())
            {
                return;
            }

            Handle<TickableEditorTask> newTask = MakeHandle<TickableEditorTask>(
                [this, readCount, updateTaskWithCount]()
                {
                    editorTask.Access([&](Handle<TickableEditorTask>& task)
                    {
                        int count = readCount();
                        const bool res = updateTaskWithCount(task, count);

                        AssertDebug(res);
                    });
                },
                "Generating terrain",
                HYP_FORMAT("{} cells", count));

            InitObject(newTask);

            newTask->SetIsForegroundTask(true);

            g_editorState->AddTask(newTask);

            task = std::move(newTask);
        });
    }
};

static TerrainGenerationEditorTaskState s_terrainGenerationEditorTask;

#endif // HYP_EDITOR

///the physics collider always uses LOD 0's full-resolution grid, regardless of the mesh LOD currently on screen
static void ExtractColliderHeights(const TerrainMeshBuilder::CellMeshData& cellMeshData, uint32 cellSize, Array<float>& outHeights)
{
    Assert(cellMeshData.numLods > 0, "No terrain mesh LODs built");

    const TerrainMeshBuilder::LodMeshData& lod0 = cellMeshData.lods[0];
    const size_t gridVertexCount = TerrainMeshHelpers::CalculateGridVertexCount(cellSize);

    Assert(lod0.vertices.Size() >= gridVertexCount, "Terrain mesh data is missing skirt vertices");

    outHeights.Resize(gridVertexCount);

    for (uint32 i = 0; i < gridVertexCount; i++)
    {
        outHeights[i] = lod0.vertices[i].GetPosition().y;
    }
}

static Handle<Texture> CreateCellTexture(Name name, uint32 cellSize, const Array<ubyte>& uploadBytes)
{
    Handle<Texture> texture = MakeHandle<Texture>();
    texture->SetName(name);

    TextureDesc textureDesc;
    textureDesc.type = TextureType::Texture2D;
    textureDesc.format = TextureFormat::RGBA8;
    textureDesc.extent = Vec3u(cellSize, cellSize, 1);
    textureDesc.filterModeMin = TextureFilterMode::Linear;
    textureDesc.filterModeMag = TextureFilterMode::Linear;

    texture->SetTextureDesc(textureDesc);
    texture->SetImageData(ConstByteView(uploadBytes.Data(), uploadBytes.Size()));
    texture->SetIsTransient(true);

    InitObject(texture);

    return texture;
}

static Handle<Texture> CreateSplatTexture(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& uploadBytes)
{
    return CreateCellTexture(NAME_FMT("TerrainCellSplatMap_{}", coord), cellSize, uploadBytes);
}

static void FlipSplatRowsForUpload(uint32 cellSize, const Array<ubyte>& splatBytes, Array<ubyte>& outUploadBytes)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    outUploadBytes.Resize(requiredSize);

    const size_t rowSize = size_t(cellSize) * 4;

    for (uint32 z = 0; z < cellSize; z++)
    {
        const size_t srcRow = size_t(cellSize - 1 - z) * rowSize;
        const size_t dstRow = size_t(z) * rowSize;

        Memory::Copy(outUploadBytes.Data() + dstRow, splatBytes.Data() + srcRow, rowSize);
    }
}

static Handle<Texture> BuildSplatTextureFromWeights(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& splatBytes)
{
    Array<ubyte> uploadBytes;
    FlipSplatRowsForUpload(cellSize, splatBytes, uploadBytes);

    return CreateSplatTexture(coord, cellSize, uploadBytes);
}

///world-space normals, row-flipped like the splat map so Terrain.hlsl samples both with the same texcoord
static void PrepareNormalMapBytes(Span<const TerrainVertex> gridVertices, uint32 cellSize, const Vec3f& scale, Array<ubyte>& outUploadBytes)
{
    const size_t texelCount = size_t(cellSize) * size_t(cellSize);

    Assert(gridVertices.Size() >= texelCount, "Not enough grid vertices for the normal map");

    Array<ubyte> normalBytes;
    normalBytes.Resize(texelCount * 4);

    const auto encodeUnorm = [](float value) -> ubyte
    {
        return ubyte(MathUtil::Clamp(value * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    for (size_t texelIndex = 0; texelIndex < texelCount; texelIndex++)
    {
        // grid normals are in the cell's local (unscaled) space
        const Vec3f localNormal = gridVertices[texelIndex].GetNormal();
        const Vec3f worldNormal = Vec3f(localNormal.x / scale.x, localNormal.y / scale.y, localNormal.z / scale.z).Normalized();

        normalBytes[texelIndex * 4] = encodeUnorm(worldNormal.x);
        normalBytes[texelIndex * 4 + 1] = encodeUnorm(worldNormal.y);
        normalBytes[texelIndex * 4 + 2] = encodeUnorm(worldNormal.z);
        normalBytes[texelIndex * 4 + 3] = 255;
    }

    FlipSplatRowsForUpload(cellSize, normalBytes, outUploadBytes);
}

///copies a painted splat map out of cell data and prepares it for upload
///call from streaming thread!
static bool PreparePaintedSplatBytes(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize, Array<ubyte>& outUploadBytes)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    Array<ubyte> splatBytes;

    {
        auto readScope = cellData->GetReadScope();

        ConstByteView splatData = cellData->GetSplatMap();

        if (splatData.Size() < requiredSize)
        {
            if (splatData.Size() != 0)
            {
                HYP_LOG(WorldGrid, Warning,
                    "Saved splat map for cell {} is {} bytes but the layer expects {} !",
                    coord,
                    splatData.Size(),
                    requiredSize);
            }

            return false;
        }

        splatBytes.Resize(requiredSize);
        Memory::Copy(splatBytes.Data(), splatData.Data(), requiredSize);
    }

    FlipSplatRowsForUpload(cellSize, splatBytes, outUploadBytes);

    return true;
}

///synthesizes auto splat weights and prepares them for upload - runs on the streaming thread.
///always sampled from LOD 0, since the splat texture is a fixed cellSize x cellSize regardless of the mesh LOD in use
static bool PrepareAutoSplatBytes(
    const TerrainGenerator& generator,
    const StreamingCellInfo& cellInfo,
    const TerrainMeshBuilder::CellMeshData& cellMeshData,
    Array<ubyte>& outUploadBytes)
{
    const uint32 cellSize = cellInfo.extent.x;
    const uint32 gridVertexCount = TerrainMeshHelpers::CalculateGridVertexCount(cellSize);

    if (cellMeshData.numLods == 0 || cellMeshData.lods[0].vertices.Size() < gridVertexCount)
    {
        return false;
    }

    const Array<TerrainVertex>& lod0Vertices = cellMeshData.lods[0].vertices;

    Array<float> heights;
    heights.Resize(gridVertexCount);

    Array<Vec3f> normals;
    normals.Resize(gridVertexCount);

    for (uint32 i = 0; i < gridVertexCount; i++)
    {
        heights[i] = lod0Vertices[i].GetPosition().y;
        normals[i] = lod0Vertices[i].GetNormal();
    }

    Array<ubyte> splatWeights;
    splatWeights.Resize(size_t(cellSize) * size_t(cellSize) * 4);

    generator.SynthesizeSplatWeights(
        heights,
        normals,
        Vec2f(cellInfo.bounds.min.x, cellInfo.bounds.min.z),
        Vec2f(cellInfo.scale.x, cellInfo.scale.z),
        cellSize,
        splatWeights);

    FlipSplatRowsForUpload(cellSize, splatWeights, outUploadBytes);

    return true;
}

static Handle<Texture> BuildPaintedSplatTexture(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    Array<ubyte> splatBytes;

    {
        auto readScope = cellData->GetReadScope();

        ConstByteView splatData = cellData->GetSplatMap();

        if (splatData.Size() < requiredSize)
        {
            if (splatData.Size() != 0)
            {
                HYP_LOG(WorldGrid, Warning,
                    "Saved splat map for cell {} is {} bytes but the layer expects {} !",
                    coord,
                    splatData.Size(),
                    requiredSize);
            }

            return Handle<Texture>::Null();
        }

        splatBytes.Resize(requiredSize);
        Memory::Copy(splatBytes.Data(), splatData.Data(), requiredSize);
    }

    return BuildSplatTextureFromWeights(coord, cellSize, splatBytes);
}

static void BuildMeshDescAndDataView(const TerrainMeshBuilder::CellMeshData& cellMeshData, MeshDesc& outMeshDesc, MeshDataView& outMeshData)
{
    outMeshDesc.meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple | VT_UV1>;

    for (uint8 lodIndex = 0; lodIndex < cellMeshData.numLods; lodIndex++)
    {
        const TerrainMeshBuilder::LodMeshData& lodMeshData = cellMeshData.lods[lodIndex];

        outMeshDesc.lods[lodIndex].numIndices = uint32(lodMeshData.indices.Size());
        outMeshDesc.lods[lodIndex].numVertices = uint32(lodMeshData.vertices.Size());
        outMeshDesc.lods[lodIndex].geometricError = lodMeshData.geometricError;

        VertexArrayView vertexArrayView {};
        vertexArrayView.floatData = reinterpret_cast<const float*>(lodMeshData.vertices.Data());
        vertexArrayView.vertexCount = lodMeshData.vertices.Size();
        vertexArrayView.layoutDesc = outMeshDesc.meshAttributes.inputLayout;

        outMeshData.vertices[lodIndex] = vertexArrayView;
        outMeshData.indices[lodIndex] = lodMeshData.indices.ToByteView();
    }
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
        s_terrainGenerationEditorTask.OnGenerationQueued();
    }
#endif
}

void TerrainStreamingCell::EndPendingGeneration()
{
#ifdef HYP_EDITOR
    if (m_isPendingGeneration.Exchange(false, MemoryOrder::ACQUIRE_RELEASE))
    {
        s_terrainGenerationEditorTask.OnGenerationFinished();
    }
#endif
}

void TerrainStreamingCell::ReleaseBuildData()
{
    EndPendingGeneration();

    m_generatedHeights = Array<float>();
    m_splatUploadBytes = Array<ubyte>();
    m_normalMapUploadBytes = Array<ubyte>();
    m_cellMeshData = TerrainMeshBuilder::CellMeshData();
}

bool TerrainStreamingCell::BuildCellMeshData(Array<float>& outGeneratedHeights)
{
    HYP_SCOPE;

    const uint32 cellSize = GetCellSize();
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    TerrainMeshBuilder meshBuilder(cellSize, m_layer->GetEffectiveLodCount(), m_layer->GetEffectiveLodStrideMultiplier());

    if (m_cellData.IsValid() && m_cellData->HasHeights())
    {
        auto cellDataReadScope = m_cellData->GetReadScope();

        const TerrainCellData& cellData = *m_cellData;
        const Span<const float> savedHeights = cellData.GetHeights();

        if (TerrainWorldGridLayer::AreCellHeightsCurrent(cellData, cellSize, m_cellFingerprint) && savedHeights.Size() == size_t(paddedSize) * size_t(paddedSize))
        {
            m_cellMeshData = meshBuilder.BuildCellMeshData(savedHeights);

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

    TerrainWorldGridLayer::GenerateCellPaddedHeights(*m_generator, m_generationLayerInfo, m_cellInfo.coord, outGeneratedHeights);

    m_cellMeshData = meshBuilder.BuildCellMeshData(outGeneratedHeights);

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

    const bool generatedHeights = BuildCellMeshData(m_generatedHeights);

    if (generatedHeights)
    {
        // already counted, unless the saved heights looked usable at creation but couldn't be paged in
        BeginPendingGeneration();
    }
    else
    {
        EndPendingGeneration();

        m_generatedHeights.Clear();
    }

    const uint32 cellSize = GetCellSize();

    ExtractColliderHeights(m_cellMeshData, cellSize, m_colliderHeights);

    PrepareNormalMapBytes(
        Span<const TerrainVertex>(m_cellMeshData.lods[0].vertices.Data(), TerrainMeshHelpers::CalculateGridVertexCount(cellSize)),
        cellSize,
        m_cellInfo.scale,
        m_normalMapUploadBytes);

    // prepare splat upload data on the streaming thread so OnLoaded() only has to create the texture object
    if (m_cellData.IsValid() && m_cellData->HasSplatMap())
    {
        if (PreparePaintedSplatBytes(m_cellData, m_cellInfo.coord, cellSize, m_splatUploadBytes))
        {
            return;
        }

        HYP_LOG(WorldGrid, Warning, "Cell {} splat data could not be loaded!", m_cellInfo.coord);
    }

    if (m_generator->GetParams().autoPaintSplats)
    {
        PrepareAutoSplatBytes(*m_generator, m_cellInfo, m_cellMeshData, m_splatUploadBytes);
    }
}

Handle<Mesh> TerrainStreamingCell::BuildMeshFromCellMeshData() const
{
    Assert(m_cellMeshData.numLods > 0 && m_cellMeshData.lods[0].vertices.Any(), "No CPU-side terrain mesh data built yet");

    MeshDesc meshDesc;
    MeshDataView meshData {};
    BuildMeshDescAndDataView(m_cellMeshData, meshDesc, meshData);

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME_FMT("TerrainChunkMesh_{}", m_cellInfo.coord));
    mesh->SetMeshData(meshDesc, meshData);
    mesh->SetIsTransient(true);
    mesh->SetIsDynamicMesh(true);
    InitObject(mesh);

    return mesh;
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

        return;
    }

    if (m_generatedHeights.Any())
    {
#ifdef HYP_EDITOR
        m_cellData = m_layer->StoreGeneratedCellHeights(m_cellInfo.coord, m_generatedHeights);
#endif

        m_generatedHeights = Array<float>();
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

        return;
    }

    const uint32 cellSize = GetCellSize();

    ///create the texture
    Handle<Texture> splatTexture;

    if (m_splatUploadBytes.Any())
    {
        splatTexture = CreateSplatTexture(m_cellInfo.coord, cellSize, m_splatUploadBytes);
    }

    m_splatUploadBytes.Clear();

    Handle<Texture> normalMapTexture;

    if (m_normalMapUploadBytes.Any())
    {
        normalMapTexture = CreateCellTexture(NAME_FMT("TerrainCellNormalMap_{}", m_cellInfo.coord), cellSize, m_normalMapUploadBytes);
    }

    m_normalMapUploadBytes.Clear();

    m_mesh = BuildMeshFromCellMeshData();

    // Free the CPU-side build data now that the GPU mesh has been created from it.
    m_cellMeshData = TerrainMeshBuilder::CellMeshData();

    HYP_LOG(WorldGrid, Verbose, "Creating terrain patch at coord {} with extent {} and scale {}, bounds: {}\tMesh Id: #{}", m_cellInfo.coord, m_cellInfo.extent, m_cellInfo.scale, m_cellInfo.bounds, m_mesh.Id().Value());

    Transform transform;
    transform.SetTranslation(m_cellInfo.bounds.min);
    transform.SetScale(m_cellInfo.scale);

    EntityInitInfo entityInitInfo {};
    entityInitInfo.bvhDepth = 0; // don't build bvhs to save load times

    m_entity = MakeHandle<Entity>(NAME_FMT("TerrainPatch_{}_Entity", m_cellInfo.coord), entityInitInfo);
    m_entity->SetLocalBounds(m_mesh->GetAABB());
    m_entity->SetIsStatic(true);

    entityManager->AddExistingEntity(m_entity);

    entityManager->GetComponent<TransformComponent>(m_entity) = TransformComponent {
        transform.GetTranslation(),
        transform.GetRotation(),
        transform.GetScale()
    };

    entityManager->GetComponent<VisibilityStateComponent>(m_entity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

    MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(m_entity);

    if (!meshComponent)
    {
        meshComponent = &entityManager->AddComponent<MeshComponent>(m_entity, MeshComponent { m_mesh, m_material });
    }
    else
    {
        meshComponent->mesh = m_mesh;
        meshComponent->material = m_material;
    }

    // most cells stream in far away, so start coarse until TerrainLodSystem picks the real LOD
    meshComponent->lodIndex = uint8(m_mesh->GetMeshDesc().GetNumLods() - 1);

    entityManager->AddComponent<TerrainCellComponent>(m_entity, TerrainCellComponent { .layer = m_layer.ToWeak() });

    m_collisionShape = MakeHandle<HeightFieldPhysicsShape>(NAME_FMT("TerrainCellCollider_{}", m_cellInfo.coord));
    InitObject(m_collisionShape);

    UpdateCollider(false /* notifyPhysicsWorld */);

    entityManager->AddComponent<RigidBodyComponent>(m_entity, RigidBodyComponent {
        .shape = m_collisionShape
    });

    m_node = m_scene->GetRoot()->AddChild();
    m_node->SetName(NAME_FMT("TerrainPatch_{}", m_cellInfo.coord));
    m_node->AddChild(m_entity);
    m_node->SetLocalTransform(transform);
    m_node->SetIsStatic(true);

    if (normalMapTexture.IsValid())
    {
        ApplyNormalMapTexture(normalMapTexture);
    }

    if (splatTexture.IsValid())
    {
        ApplySplatTexture(splatTexture);
    }

    m_layer->RegisterLoadedCell(m_cellInfo.coord, WeakHandleFromThis());
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
    m_colliderHeights.Clear();
}

void TerrainStreamingCell::DetachFromScene()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_node.IsValid())
    {
        return;
    }

    m_node->Remove(/* moveToDetached */ false);
    m_node.Reset();

    m_entity.Reset();
}

void TerrainStreamingCell::UpdateSplatMaterial(const Handle<TerrainCellData>& cellData)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");
    Assert(m_mesh.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    const uint32 cellSize = GetCellSize();

    if (!cellData.IsValid() || !cellData->HasSplatMap() || !m_material.IsValid())
    {
        return;
    }

    Handle<Texture> splatTexture = BuildPaintedSplatTexture(cellData, m_cellInfo.coord, cellSize);

    if (!splatTexture.IsValid())
    {
        return;
    }

    ApplySplatTexture(splatTexture);
}

void TerrainStreamingCell::RefreshAutoSplat()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_layer.IsValid() || !m_mesh.IsValid())
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

    const uint32 cellSize = GetCellSize();
    const uint32 gridVertexCount = TerrainMeshHelpers::CalculateGridVertexCount(cellSize);

    const VertexArrayView vertexData = m_mesh->GetVertexData(0);

    if (vertexData.floatData == nullptr || vertexData.vertexCount < gridVertexCount)
    {
        return;
    }

    const Span<const TerrainVertex> gridVertices(reinterpret_cast<const TerrainVertex*>(vertexData.floatData), gridVertexCount);

    Array<float> heights;
    heights.Resize(gridVertexCount);

    Array<Vec3f> normals;
    normals.Resize(gridVertexCount);

    for (uint32 i = 0; i < gridVertexCount; i++)
    {
        heights[i] = gridVertices[i].GetPosition().y;
        normals[i] = gridVertices[i].GetNormal();
    }

    Array<ubyte> splatWeights;
    splatWeights.Resize(size_t(cellSize) * size_t(cellSize) * 4);

    m_generator->SynthesizeSplatWeights(
        heights,
        normals,
        Vec2f(m_cellInfo.bounds.min.x, m_cellInfo.bounds.min.z),
        Vec2f(m_cellInfo.scale.x, m_cellInfo.scale.z),
        cellSize,
        splatWeights);

    ApplySplatTexture(BuildSplatTextureFromWeights(m_cellInfo.coord, cellSize, splatWeights));
}

void TerrainStreamingCell::ApplySplatTexture(const Handle<Texture>& splatTexture)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");
    Assert(m_mesh.IsValid(), "Cell has not finished loading yet");

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

void TerrainStreamingCell::RefreshNormalMap(Span<const TerrainVertex> gridVertices)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 cellSize = GetCellSize();

    Array<ubyte> normalMapBytes;
    PrepareNormalMapBytes(gridVertices, cellSize, m_cellInfo.scale, normalMapBytes);

    ApplyNormalMapTexture(CreateCellTexture(NAME_FMT("TerrainCellNormalMap_{}", m_cellInfo.coord), cellSize, normalMapBytes));
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

    if (MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(m_entity))
    {
        meshComponent->material = m_cellMaterial;
    }

    // the replaced texture is still referenced by static shadow views that skipped collection
    m_scene->MarkStaticRenderResourcesChanged();

    m_entity->SetNeedsRenderProxyUpdate();
    m_entity->MarkDirty();
}

void TerrainStreamingCell::RebuildMeshFull(const Handle<TerrainCellData>& cellData)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");
    Assert(m_mesh.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    Array<float> generatedHeights;

    if (BuildCellMeshData(generatedHeights))
    {
#ifdef HYP_EDITOR
        m_cellData = m_layer->StoreGeneratedCellHeights(m_cellInfo.coord, generatedHeights);
#endif
    }

    const uint32 cellSize = GetCellSize();

    ExtractColliderHeights(m_cellMeshData, cellSize, m_colliderHeights);

    MeshDesc meshDesc;
    MeshDataView meshData {};
    BuildMeshDescAndDataView(m_cellMeshData, meshDesc, meshData);

    m_mesh->SetMeshData(meshDesc, meshData);

    // Don't UploadGpuData() yet - allow the ResourceBinder to do that on the render thread
    // as we need it to prevent stalls

    RefreshNormalMap(Span<const TerrainVertex>(m_cellMeshData.lods[0].vertices.Data(), TerrainMeshHelpers::CalculateGridVertexCount(cellSize)));

    m_cellMeshData = TerrainMeshBuilder::CellMeshData();

    m_entity->SetLocalBounds(m_mesh->GetAABB());

    UpdateCollider(true /* notifyPhysicsWorld */);

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        return;
    }

    m_entity->SetNeedsRenderProxyUpdate();
}

void TerrainStreamingCell::RebuildMesh(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");
    Assert(m_mesh.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    const uint32 cellSize = GetCellSize();

    if (m_mesh->GetMeshDesc().GetNumLods() > 1)
    {
        // The fast path below only patches LOD 0 in place. With multiple mesh LODs, a partial edit would leave
        // LOD 1+ (and LOD 0's own morph targets, which point at LOD 1's heights) stale until some other full
        // rebuild happened to come along, which can show as a crack or a misplaced morph the next time this
        // cell's LOD changes. A full rebuild is cheap (a few thousand vertices), so always do that instead.
        
        RebuildMeshFull(cellData);

        return;
    }

    const VertexArrayView vertexData = m_mesh->GetVertexData(0);

    if (vertexData.floatData == nullptr || vertexData.vertexCount < TerrainMeshHelpers::CalculateGridVertexCount(cellSize))
    {
        HYP_LOG(WorldGrid, Warning, "Cell {} has invalid vertex data for terrain", m_cellInfo.coord);

        RebuildMeshFull(cellData);

        return;
    }

    const int32 minVertexX = MathUtil::Clamp(minVertex.x, 0, int32(cellSize) - 1);
    const int32 maxVertexX = MathUtil::Clamp(maxVertex.x, 0, int32(cellSize) - 1);
    const int32 minVertexZ = MathUtil::Clamp(minVertex.y, 0, int32(cellSize) - 1);
    const int32 maxVertexZ = MathUtil::Clamp(maxVertex.y, 0, int32(cellSize) - 1);

    if (minVertexX > maxVertexX || minVertexZ > maxVertexZ)
    {
        return;
    }

    const int32 updateMinX = MathUtil::Max(minVertexX - 1, 0);
    const int32 updateMaxX = MathUtil::Min(maxVertexX + 1, int32(cellSize) - 1);
    const int32 updateMinZ = MathUtil::Max(minVertexZ - 1, 0);
    const int32 updateMaxZ = MathUtil::Min(maxVertexZ + 1, int32(cellSize) - 1);

    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    const bool hasCurrentHeights = m_cellData.IsValid() && TerrainWorldGridLayer::AreCellHeightsCurrent(*m_cellData, cellSize, m_layer->GetCellFingerprint());

    if (!hasCurrentHeights)
    {
        RebuildMeshFull(cellData);

        return;
    }

    auto cellDataReadScope = m_cellData->GetReadScope();

    const Span<const float> paddedHeights = static_cast<const TerrainCellData&>(*m_cellData).GetHeights();

    if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        HYP_LOG(WorldGrid, Warning, "Cell {} heights could not be paged in for a partial rebuild", m_cellInfo.coord);

        return;
    }

    // the padding ring holds the neighbor's border-adjacent heights, so border normals stay consistent across seams
    const auto heightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(TerrainGenerator::CellPadding)) * paddedSize + size_t(x + int32(TerrainGenerator::CellPadding))];
    };

    const uint32 firstVertex = uint32(updateMinZ) * cellSize;
    const uint32 numRows = uint32(updateMaxZ - updateMinZ + 1);
    const uint32 numVertices = numRows * cellSize;

    const uint32 vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

    m_scratchVertices.Resize(numVertices);

    Memory::Copy(
        m_scratchVertices.Data(),
        vertexData.floatData + (firstVertex * vertexSizeInFloats),
        numVertices * vertexSizeInFloats * sizeof(float));

    for (int32 z = minVertexZ; z <= maxVertexZ; z++)
    {
        for (int32 x = minVertexX; x <= maxVertexX; x++)
        {
            m_scratchVertices[size_t(z - updateMinZ) * cellSize + x].SetPosition(Vec3f { float(x), heightAt(x, z), float(z) });
        }
    }

    if (m_collisionShape.IsValid() && m_colliderHeights.Size() == size_t(cellSize) * size_t(cellSize))
    {
        for (int32 z = minVertexZ; z <= maxVertexZ; z++)
        {
            for (int32 x = minVertexX; x <= maxVertexX; x++)
            {
                m_colliderHeights[size_t(z) * size_t(cellSize) + size_t(x)] = heightAt(x, z);
            }
        }
    }

    for (int32 z = updateMinZ; z <= updateMaxZ; z++)
    {
        for (int32 x = updateMinX; x <= updateMaxX; x++)
        {
            m_scratchVertices[size_t(z - updateMinZ) * cellSize + x].SetNormal(TerrainGenerator::ComputeGridNormal(
                heightAt(x - 1, z),
                heightAt(x + 1, z),
                heightAt(x, z - 1),
                heightAt(x, z + 1)));
        }
    }

    VertexArrayView rangeView {};
    rangeView.floatData = reinterpret_cast<const float*>(m_scratchVertices.Data());
    rangeView.vertexCount = numVertices;
    rangeView.layoutDesc = vertexData.layoutDesc;

    m_mesh->UpdateDynamicVertexData(0, firstVertex, rangeView);

    // rebuild skirts
    const uint32 gridVertexCount = TerrainMeshHelpers::CalculateGridVertexCount(cellSize);
    const uint32 skirtVertexCount = TerrainMeshHelpers::CalculateSkirtVertexCount(cellSize);

    m_scratchVertices.Resize(skirtVertexCount);

    TerrainMeshHelpers::BuildSkirtVertices(
        cellSize,
        Span<const TerrainVertex>(reinterpret_cast<const TerrainVertex*>(vertexData.floatData), gridVertexCount),
        m_scratchVertices);

    VertexArrayView skirtRangeView {};
    skirtRangeView.floatData = reinterpret_cast<const float*>(m_scratchVertices.Data());
    skirtRangeView.vertexCount = skirtVertexCount;
    skirtRangeView.layoutDesc = vertexData.layoutDesc;

    m_mesh->UpdateDynamicVertexData(0, gridVertexCount, skirtRangeView);

    RefreshNormalMap(Span<const TerrainVertex>(reinterpret_cast<const TerrainVertex*>(vertexData.floatData), gridVertexCount));

    m_entity->SetLocalBounds(m_mesh->GetAABB());

    UpdateCollider(true /* notifyPhysicsWorld */);

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        return;
    }

    m_entity->SetNeedsRenderProxyUpdate();
}

void TerrainStreamingCell::UpdateCollider(bool notifyPhysicsWorld)
{
    HYP_SCOPE;

    if (!m_collisionShape.IsValid() || !m_layer.IsValid() || !m_colliderHeights.Any())
    {
        return;
    }

    m_collisionShape->SetHeights(m_colliderHeights, GetCellSize());

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

void TerrainStreamingCell::RebuildPickBVH()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_mesh.IsValid())
    {
        return;
    }

    m_mesh->UpdateDynamicBVH();
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

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
