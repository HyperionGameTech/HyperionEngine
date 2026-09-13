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

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <TerrainStreamingCell.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static void ExtractColliderHeights(const TerrainMeshBuilder::CellMeshData& cellMeshData, uint32 cellSize, Array<float>& outHeights)
{
    const size_t gridVertexCount = TerrainMeshBuilder::CalculateGridVertexCount(cellSize);

    Assert(cellMeshData.vertices.Size() >= gridVertexCount, "Terrain mesh data is missing skirt vertices");

    outHeights.Resize(gridVertexCount);

    for (uint32 i = 0; i < gridVertexCount; i++)
    {
        outHeights[i] = cellMeshData.vertices[i].GetPosition().y;
    }
}

static Handle<Texture> CreateSplatTexture(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& uploadBytes)
{
    Handle<Texture> texture = MakeHandle<Texture>();
    texture->SetName(NAME_FMT("TerrainCellSplatMap_{}", coord));

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

static Handle<Texture> BuildSplatTextureFromWeights(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& splatBytes)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    Array<ubyte> uploadBytes;
    uploadBytes.Resize(requiredSize);

    const size_t rowSize = size_t(cellSize) * 4;

    for (uint32 z = 0; z < cellSize; z++)
    {
        const size_t srcRow = size_t(cellSize - 1 - z) * rowSize;
        const size_t dstRow = size_t(z) * rowSize;

        Memory::Copy(uploadBytes.Data() + dstRow, splatBytes.Data() + srcRow, rowSize);
    }

    return CreateSplatTexture(coord, cellSize, uploadBytes);
}

static Handle<Texture> BuildAutoSplatTexture(
    const Handle<TerrainWorldGridLayer>& layer,
    const StreamingCellInfo& cellInfo,
    const TerrainMeshBuilder::CellMeshData& cellMeshData)
{
    const WorldGridLayerInfo& layerInfo = layer->GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;
    const uint32 gridVertexCount = TerrainMeshBuilder::CalculateGridVertexCount(cellSize);

    Assert(cellMeshData.vertices.Size() >= gridVertexCount, "Terrain mesh data is missing skirt vertices");

    Array<float> heights;
    heights.Resize(gridVertexCount);

    Array<Vec3f> normals;
    normals.Resize(gridVertexCount);

    for (uint32 i = 0; i < gridVertexCount; i++)
    {
        heights[i] = cellMeshData.vertices[i].GetPosition().y;
        normals[i] = cellMeshData.vertices[i].GetNormal();
    }

    Array<ubyte> splatWeights;
    splatWeights.Resize(size_t(cellSize) * size_t(cellSize) * 4);

    layer->GetGenerator().SynthesizeSplatWeights(
        heights,
        normals,
        Vec2f(cellInfo.bounds.min.x, cellInfo.bounds.min.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        cellSize,
        splatWeights);

    return BuildSplatTextureFromWeights(cellInfo.coord, cellSize, splatWeights);
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
    outMeshDesc.meshAttributes.inputLayout = { VT_Simple };
    outMeshDesc.lods[0].numIndices = uint32(cellMeshData.indices.Size());
    outMeshDesc.lods[0].numVertices = uint32(cellMeshData.vertices.Size());

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(cellMeshData.vertices.Data());
    vertexArrayView.vertexCount = cellMeshData.vertices.Size();
    vertexArrayView.layoutDesc = outMeshDesc.meshAttributes.inputLayout;

    outMeshData.vertices[0] = vertexArrayView;
    outMeshData.indices[0] = cellMeshData.indices.ToByteView();
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
    const Handle<TerrainCellData>& cellData)
    : StreamingCell(cellInfo),
      m_scene(scene),
      m_material(material),
      m_layer(layer),
      m_cellData(cellData)
{
}

TerrainStreamingCell::~TerrainStreamingCell() = default;

void TerrainStreamingCell::OnStreamStart()
{
    HYP_SCOPE;

    Assert(m_layer.IsValid(), "Invalid terrain layer!");

    const uint32 cellSize = m_layer->GetLayerInfo().cellSize;
    TerrainMeshBuilder meshBuilder(cellSize);

    if (!m_cellData.IsValid())
    {
        m_cellMeshData = meshBuilder.BuildCellVertexData(m_cellInfo, m_layer->GetGenerator(), Span<const float>());
    }
    else
    {
        const bool expectSculptData = m_cellData->HasSculptDelta();

        Span<const float> sculptDelta;

        {
            auto cellDataReadScope = m_cellData->GetReadScope();

            sculptDelta = m_cellData->GetSculptDeltaFloat();

            const size_t expectedCount = size_t(cellSize) * size_t(cellSize);

            if (sculptDelta.Size() != expectedCount)
            {
                if (sculptDelta.Size() == 0)
                {
                    if (expectSculptData)
                    {
                        HYP_LOG(WorldGrid, Warning,
                            "Cell {} has saved sculpt data but it could not be paged in - cell will render without sculpt edits",
                            m_cellInfo.coord);
                    }
                }
                else
                {
                    HYP_LOG(WorldGrid, Warning,
                        "Saved sculpt data for cell {} has {} vertices but the layer expects {} - likely saved with a different cell size, ignoring it",
                        m_cellInfo.coord,
                        sculptDelta.Size(),
                        expectedCount);
                }

                sculptDelta = {};
            }

            m_cellMeshData = meshBuilder.BuildCellVertexData(m_cellInfo, m_layer->GetGenerator(), sculptDelta);
        }
    }

    ExtractColliderHeights(m_cellMeshData, cellSize, m_colliderHeights);
}

Handle<Mesh> TerrainStreamingCell::BuildMeshFromCellMeshData() const
{
    Assert(m_cellMeshData.vertices.Any(), "No CPU-side terrain mesh data built yet");

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

    const uint32 cellSize = m_layer->GetLayerInfo().cellSize;

    Handle<Texture> splatTexture;

    if (m_cellData.IsValid() && m_cellData->HasSplatMap())
    {
        splatTexture = BuildPaintedSplatTexture(m_cellData, m_cellInfo.coord, cellSize);

        if (!splatTexture.IsValid())
        {
            HYP_LOG(WorldGrid, Warning,
                "Cell {} has painted splat data but it could not be loaded - falling back to auto painting, visuals will differ from the painted result",
                m_cellInfo.coord);
        }
    }

    if (!splatTexture.IsValid()
        && m_layer->GetGenerator().GetParams().autoPaintSplats
        && m_cellMeshData.vertices.Size() >= TerrainMeshBuilder::CalculateGridVertexCount(cellSize))
    {
        splatTexture = BuildAutoSplatTexture(m_layer, m_cellInfo, m_cellMeshData);
    }

    m_mesh = BuildMeshFromCellMeshData();

    // Free the CPU-side build data now that the GPU mesh has been created from it.
    m_cellMeshData = TerrainMeshBuilder::CellMeshData();

    HYP_LOG(WorldGrid, Verbose, "Creating terrain patch at coord {} with extent {} and scale {}, bounds: {}\tMesh Id: #{}", m_cellInfo.coord, m_cellInfo.extent, m_cellInfo.scale, m_cellInfo.bounds, m_mesh.Id().Value());

    Transform transform;
    transform.SetTranslation(m_cellInfo.bounds.min);
    transform.SetScale(m_cellInfo.scale);

    m_entity = entityManager->AddEntity();
    m_entity->SetLocalBounds(m_mesh->GetAABB());
    m_entity->SetIsStatic(true);

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

    entityManager->AddComponent<TerrainCellComponent>(m_entity, TerrainCellComponent {});

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
    
    const bool isAddedToLayer = m_node.IsValid();

    if (isAddedToLayer)
    {
        if (m_layer.IsValid())
        {
            m_layer->UnregisterLoadedCell(m_cellInfo.coord);
        }

        m_node->Remove(/* moveToDetached */ false);
        m_node.Reset();

        m_entity.Reset();
    }

    m_splatTexture.Reset();
    m_cellMaterial.Reset();

    m_collisionShape.Reset();
    m_colliderHeights.Clear();
}

void TerrainStreamingCell::UpdateSplatMaterial(const Handle<TerrainCellData>& cellData)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_layer.IsValid(), "Invalid terrain layer!");
    Assert(m_entity.IsValid(), "Cell has not finished loading yet");
    Assert(m_mesh.IsValid(), "Cell has not finished loading yet");

    m_cellData = cellData;

    const uint32 cellSize = m_layer->GetLayerInfo().cellSize;

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

    if (!m_layer->GetGenerator().GetParams().autoPaintSplats)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layer->GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;
    const uint32 gridVertexCount = TerrainMeshBuilder::CalculateGridVertexCount(cellSize);

    const VertexArrayView vertexData = m_mesh->GetVertexData(0);

    if (vertexData.floatData == nullptr || vertexData.vertexCount < gridVertexCount)
    {
        return;
    }

    const Span<const SimpleVertex> gridVertices(reinterpret_cast<const SimpleVertex*>(vertexData.floatData), gridVertexCount);

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

    m_layer->GetGenerator().SynthesizeSplatWeights(
        heights,
        normals,
        Vec2f(m_cellInfo.bounds.min.x, m_cellInfo.bounds.min.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
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

    if (!m_cellMaterial.IsValid())
    {
        m_cellMaterial = m_material->Clone();
        m_cellMaterial->SetName(NAME_FMT("TerrainCellMaterial_{}", m_cellInfo.coord));
        InitObject(m_cellMaterial);
    }

    m_cellMaterial->SetTexture(MaterialTextureKey::TerrainSplatMap, m_splatTexture);

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entityManager.IsValid())
    {
        return;
    }

    if (MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(m_entity))
    {
        meshComponent->material = m_cellMaterial;
    }

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

    TerrainMeshBuilder meshBuilder(m_layer->GetLayerInfo().cellSize);

    if (!m_cellData.IsValid())
    {
        m_cellMeshData = meshBuilder.BuildCellVertexData(m_cellInfo, m_layer->GetGenerator(), Span<const float>());
    }
    else
    {
        auto cellDataReadScope = m_cellData->GetReadScope();

        m_cellMeshData = meshBuilder.BuildCellVertexData(m_cellInfo, m_layer->GetGenerator(), m_cellData->GetSculptDeltaFloat());
    }

    ExtractColliderHeights(m_cellMeshData, m_layer->GetLayerInfo().cellSize, m_colliderHeights);

    MeshDesc meshDesc;
    MeshDataView meshData {};
    BuildMeshDescAndDataView(m_cellMeshData, meshDesc, meshData);

    m_mesh->SetMeshData(meshDesc, meshData);

    // Don't UploadGpuData() yet - allow the ResourceBinder to do that on the render thread
    // as we need it to prevent stalls

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

    const WorldGridLayerInfo& layerInfo = m_layer->GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;

    const VertexArrayView vertexData = m_mesh->GetVertexData(0);

    if (vertexData.floatData == nullptr || vertexData.vertexCount < TerrainMeshBuilder::CalculateGridVertexCount(cellSize))
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

    // normals consistent across cell seams.
    const int32 heightsMinX = updateMinX - 1;
    const int32 heightsMaxX = updateMaxX + 1;
    const int32 heightsMinZ = updateMinZ - 1;
    const int32 heightsMaxZ = updateMaxZ + 1;

    const int32 heightsWidth = heightsMaxX - heightsMinX + 1;
    const int32 heightsDepth = heightsMaxZ - heightsMinZ + 1;

    m_scratchHeights.Resize(size_t(heightsWidth) * size_t(heightsDepth));

    const Vec2f cellWorldMinXZ(m_cellInfo.bounds.min.x, m_cellInfo.bounds.min.z);
    const Vec2f scaleXZ(layerInfo.scale.x, layerInfo.scale.z);

    for (int32 z = heightsMinZ; z <= heightsMaxZ; z++)
    {
        for (int32 x = heightsMinX; x <= heightsMaxX; x++)
        {
            const Vec2f worldXZ = cellWorldMinXZ + Vec2f(float(x), float(z)) * scaleXZ;

            m_scratchHeights[size_t(z - heightsMinZ) * size_t(heightsWidth) + size_t(x - heightsMinX)] = m_layer->SampleHeightAt(worldXZ);
        }
    }

    const auto heightAt = [&](int32 x, int32 z) -> float
    {
        return m_scratchHeights[size_t(z - heightsMinZ) * size_t(heightsWidth) + size_t(x - heightsMinX)];
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
    const uint32 gridVertexCount = TerrainMeshBuilder::CalculateGridVertexCount(cellSize);
    const uint32 skirtVertexCount = TerrainMeshBuilder::CalculateSkirtVertexCount(cellSize);

    m_scratchVertices.Resize(skirtVertexCount);

    TerrainMeshBuilder::BuildSkirtVertices(
        cellSize,
        Span<const SimpleVertex>(reinterpret_cast<const SimpleVertex*>(vertexData.floatData), gridVertexCount),
        m_scratchVertices);

    VertexArrayView skirtRangeView {};
    skirtRangeView.floatData = reinterpret_cast<const float*>(m_scratchVertices.Data());
    skirtRangeView.vertexCount = skirtVertexCount;
    skirtRangeView.layoutDesc = vertexData.layoutDesc;

    m_mesh->UpdateDynamicVertexData(0, gridVertexCount, skirtRangeView);

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

    m_collisionShape->SetHeights(m_colliderHeights, m_layer->GetLayerInfo().cellSize);

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

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
