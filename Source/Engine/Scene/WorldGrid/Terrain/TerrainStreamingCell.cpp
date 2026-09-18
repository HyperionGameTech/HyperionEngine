/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>
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
static void PrepareNormalMapBytes(Span<const float> paddedHeights, Span<const ubyte> erosionMasks, uint32 cellSize, const Vec3f& scale, Array<ubyte>& outUploadBytes)
{
    const size_t texelCount = size_t(cellSize) * size_t(cellSize);
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    const bool hasErosionMasks = erosionMasks.Size() == texelCount * TerrainErosionMasks::NumChannels;

    Array<float> heights;
    Array<Vec3f> localNormals;
    TerrainGenerator::ExtractCellHeightsAndNormals(paddedHeights, cellSize, heights, localNormals);

    const auto paddedHeightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(TerrainGenerator::CellPadding)) * paddedSize + size_t(x + int32(TerrainGenerator::CellPadding))];
    };

    Array<ubyte> normalBytes;
    normalBytes.Resize(texelCount * 4);

    const auto encodeUnorm = [](float value) -> ubyte
    {
        return ubyte(MathUtil::Clamp(value * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    const float sampleSpacing = MathUtil::Max((scale.x + scale.z) * 0.5f, 0.0001f);

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const size_t texelIndex = size_t(z) * cellSize + x;

            // grid normals are in the cell's local (unscaled) space
            const Vec3f localNormal = localNormals[texelIndex];
            const Vec3f worldNormal = Vec3f(localNormal.x / scale.x, localNormal.y / scale.y, localNormal.z / scale.z).Normalized();

            // sculpted detail the erosion masks never saw still reads as hollows and bumps
            const float neighborMean = (paddedHeightAt(int32(x) - 1, int32(z))
                + paddedHeightAt(int32(x) + 1, int32(z))
                + paddedHeightAt(int32(x), int32(z) - 1)
                + paddedHeightAt(int32(x), int32(z) + 1))
                * 0.25f;

            float concavity = (neighborMean - heights[texelIndex]) / sampleSpacing;

            if (hasErosionMasks)
            {
                concavity += TerrainErosionMasks::DecodeConcavity(erosionMasks[texelIndex * TerrainErosionMasks::NumChannels + TerrainErosionMasks::ConcavityChannel]);
            }

            normalBytes[texelIndex * 4] = encodeUnorm(worldNormal.x);
            normalBytes[texelIndex * 4 + 1] = encodeUnorm(worldNormal.y);
            normalBytes[texelIndex * 4 + 2] = encodeUnorm(worldNormal.z);
            normalBytes[texelIndex * 4 + 3] = ubyte(TerrainErosionMasks::EncodeConcavity(concavity) * 255.0f + 0.5f);
        }
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

///synthesizes auto splat weights and prepares them for upload.
///always sampled at full resolution, since the splat texture is a fixed cellSize x cellSize regardless of the mesh LOD in use
static bool PrepareAutoSplatBytes(
    const TerrainGenerator& generator,
    const StreamingCellInfo& cellInfo,
    Span<const float> paddedHeights,
    Span<const ubyte> erosionMasks,
    Array<ubyte>& outUploadBytes)
{
    const uint32 cellSize = cellInfo.extent.x;
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        return false;
    }

    Array<ubyte> splatWeights;
    splatWeights.Resize(size_t(cellSize) * size_t(cellSize) * 4);

    generator.SynthesizeSplatWeights(
        paddedHeights,
        erosionMasks,
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

    m_hasGeneratedHeights = false;
    m_splatUploadBytes = Array<ubyte>();
    m_normalMapUploadBytes = Array<ubyte>();
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

    PrepareNormalMapBytes(m_paddedHeights, m_erosionMasks, cellSize, m_cellInfo.scale, m_normalMapUploadBytes);

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
        PrepareAutoSplatBytes(*m_generator, m_cellInfo, m_paddedHeights, m_erosionMasks, m_splatUploadBytes);
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

    if (normalMapTexture.IsValid())
    {
        ApplyNormalMapTexture(normalMapTexture);
    }

    if (splatTexture.IsValid())
    {
        ApplySplatTexture(splatTexture);
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
}

void TerrainStreamingCell::UpdateSplatMaterial(const Handle<TerrainCellData>& cellData)
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

    if (!PrepareAutoSplatBytes(*m_generator, m_cellInfo, m_paddedHeights, m_erosionMasks, splatUploadBytes))
    {
        return;
    }

    ApplySplatTexture(CreateSplatTexture(m_cellInfo.coord, GetCellSize(), splatUploadBytes));
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
    PrepareNormalMapBytes(m_paddedHeights, m_erosionMasks, cellSize, m_cellInfo.scale, normalMapBytes);

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

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
