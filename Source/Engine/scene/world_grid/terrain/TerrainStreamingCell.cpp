/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/world_grid/terrain/TerrainStreamingCell.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/Scene.hpp>
#include <scene/Node.hpp>
#include <scene/World.hpp>

#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Vertex.hpp>

#include <Core/io/ByteWriter.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <engine/EngineGlobals.hpp>

#include <util/NoiseFactory.hpp>

#include <TerrainStreamingCell.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static constexpr float BaseHeight = 5.0f;
static constexpr float MountainHeight = 35.0f;
static constexpr float NoiseScale = 1.0f;

namespace terrain {

struct TerrainHeight
{
    float height;
    float Erosion;
    float sediment;
    float water;
    float newWater;
    float displacement;
};

struct TerrainHeightData
{
    StreamingCellInfo cellInfo;
    Array<TerrainHeight> heights;

    TerrainHeightData(const StreamingCellInfo& cellInfo)
        : cellInfo(cellInfo)
    {
        heights.Resize(cellInfo.extent.x * cellInfo.extent.z);
    }

    TerrainHeightData(const TerrainHeightData& other) = delete;
    TerrainHeightData& operator=(const TerrainHeightData& other) = delete;
    TerrainHeightData(TerrainHeightData&& other) noexcept = delete;
    TerrainHeightData& operator=(TerrainHeightData&& other) noexcept = delete;
    ~TerrainHeightData() = default;

    uint32 GetHeightIndex(int x, int z) const
    {
        return uint32(((x + cellInfo.extent.x) % cellInfo.extent.x)
            + ((z + cellInfo.extent.z) % cellInfo.extent.z) * cellInfo.extent.x);
    }
};

class TerrainErosion
{
    static constexpr uint32 NumIterations = 250u;
    static constexpr float ErosionScale = 0.05f;
    static constexpr float Evaporation = 0.9f;
    static constexpr float Erosion = 0.004f * ErosionScale;
    static constexpr float Deposition = 0.0000002f * ErosionScale;

    static constexpr FixedArray<Pair<int, int>, 8> Offsets = {
        Pair<int, int> { 1, 0 },
        Pair<int, int> { 1, 1 },
        Pair<int, int> { 1, -1 },
        Pair<int, int> { 0, 1 },
        Pair<int, int> { 0, -1 },
        Pair<int, int> { -1, 0 },
        Pair<int, int> { -1, 1 },
        Pair<int, int> { -1, -1 }
    };

public:
    static void Erode(TerrainHeightData& heightData);
};

void TerrainErosion::Erode(TerrainHeightData& heightData)
{
    for (uint32 iteration = 0; iteration < NumIterations; iteration++)
    {
        for (int z = 1; z < heightData.cellInfo.extent.z - 2; z++)
        {
            for (int x = 1; x < heightData.cellInfo.extent.x - 2; x++)
            {
                TerrainHeight& heightInfo = heightData.heights[heightData.GetHeightIndex(x, z)];
                heightInfo.displacement = 0.0f;

                for (const auto& offset : Offsets)
                {
                    const auto& neighborHeightInfo = heightData.heights[heightData.GetHeightIndex(x + offset.first, z + offset.second)];

                    heightInfo.displacement += MathUtil::Max(heightInfo.height - neighborHeightInfo.height, 0.0f);
                }

                if (heightInfo.displacement != 0.0f)
                {
                    float water = heightInfo.water * Evaporation;
                    float stayingWater = (water * 0.0002f) / (heightInfo.displacement * ErosionScale + 1);
                    water -= stayingWater;

                    for (const auto& offset : Offsets)
                    {
                        auto& neighborHeightInfo = heightData.heights[heightData.GetHeightIndex(x + offset.first, z + offset.second)];

                        neighborHeightInfo.newWater += MathUtil::Max(heightInfo.height - neighborHeightInfo.height, 0.0f) / heightInfo.displacement * water;
                    }

                    heightInfo.water = stayingWater + 1.0f;
                }
            }
        }

        for (int z = 1; z < heightData.cellInfo.extent.z - 2; z++)
        {
            for (int x = 1; x < heightData.cellInfo.extent.x - 2; x++)
            {
                TerrainHeight& heightInfo = heightData.heights[heightData.GetHeightIndex(x, z)];

                heightInfo.water += heightInfo.newWater;
                heightInfo.newWater = 0.0f;

                const float oldHeight = heightInfo.height;
                heightInfo.height += (-(heightInfo.displacement - (0.005f / ErosionScale)) * heightInfo.water) * Erosion + heightInfo.water * Deposition;
                heightInfo.Erosion = oldHeight - heightInfo.height;

                if (oldHeight < heightInfo.height)
                {
                    heightInfo.water = MathUtil::Max(heightInfo.water - (heightInfo.height - oldHeight) * 1000.0f, 0.0f);
                }
            }
        }
    }
}

class TerrainMeshBuilder
{
public:
    TerrainMeshBuilder(const StreamingCellInfo& cellInfo);
    TerrainMeshBuilder(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder& operator=(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder(TerrainMeshBuilder&& other) noexcept = delete;
    TerrainMeshBuilder& operator=(TerrainMeshBuilder&& other) noexcept = delete;
    ~TerrainMeshBuilder() = default;

    void GenerateHeights(const NoiseCombinator& noiseCombinator);
    Handle<Mesh> BuildMesh() const;

private:
    Array<Vertex> BuildVertices() const;
    Array<uint32> BuildIndices() const;

    TerrainHeightData m_heightData;
};

TerrainMeshBuilder::TerrainMeshBuilder(const StreamingCellInfo& cellInfo)
    : m_heightData(cellInfo)
{
}

void TerrainMeshBuilder::GenerateHeights(const NoiseCombinator& noiseCombinator)
{
    HYP_SCOPE;

    for (int z = 0; z < int(m_heightData.cellInfo.extent.z); z++)
    {
        for (int x = 0; x < int(m_heightData.cellInfo.extent.x); x++)
        {
            const float xOffset = float(x + (m_heightData.cellInfo.coord.x * int(m_heightData.cellInfo.extent.x - 1))) / float(m_heightData.cellInfo.extent.x);
            const float zOffset = float(z + (m_heightData.cellInfo.coord.y * int(m_heightData.cellInfo.extent.z - 1))) / float(m_heightData.cellInfo.extent.z);

            const uint32 index = m_heightData.GetHeightIndex(x, z);

            m_heightData.heights[index] = TerrainHeight {
                .height = noiseCombinator.GetNoise(Vec2f(xOffset, zOffset)),
                .Erosion = 0.0f,
                .sediment = 0.0f,
                .water = 1.0f,
                .newWater = 0.0f,
                .displacement = 0.0f
            };
        }
    }

    auto bm = noiseCombinator.CreateBitmap(
        Vec2f(
            float(m_heightData.cellInfo.coord.x * int(m_heightData.cellInfo.extent.x - 1)),
            float(m_heightData.cellInfo.coord.y * int(m_heightData.cellInfo.extent.z - 1))),
        Vec2u(m_heightData.cellInfo.extent.x, m_heightData.cellInfo.extent.z),
        NoiseScale);

    FileByteWriter fileWriter { HYP_FORMAT("Temp/terrain_noise_{}_{}.bmp",
        m_heightData.cellInfo.coord.x,
        m_heightData.cellInfo.coord.y) };

    if (!bm.Write(&fileWriter))
    {
        HYP_LOG(WorldGrid, Error, "Failed to write terrain noise bitmap!");
    }

    TerrainErosion::Erode(m_heightData);
}

Handle<Mesh> TerrainMeshBuilder::BuildMesh() const
{
    HYP_SCOPE;

    Array<Vertex> vertices = BuildVertices();
    Array<uint32> indices = BuildIndices();

    MeshDesc meshDesc;
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetMeshData(meshDesc, vertices.ToSpan(), indices.ToByteView());

    mesh->CalculateNormals();

    return mesh;
}

Array<Vertex> TerrainMeshBuilder::BuildVertices() const
{
    Array<Vertex> vertices;
    vertices.Resize(m_heightData.cellInfo.extent.x * m_heightData.cellInfo.extent.z);

    int i = 0;

    for (int z = 0; z < m_heightData.cellInfo.extent.z; z++)
    {
        for (int x = 0; x < m_heightData.cellInfo.extent.x; x++)
        {
            const Vec3f position = Vec3f { float(x), m_heightData.heights[i].height, float(z) } * m_heightData.cellInfo.scale;

            const Vec2f texcoord(
                float(x) / float(m_heightData.cellInfo.extent.x),
                float(z) / float(m_heightData.cellInfo.extent.z));

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

Array<uint32> TerrainMeshBuilder::BuildIndices() const
{
    Array<uint32> indices;
    indices.Resize(SizeType(6 * (m_heightData.cellInfo.extent.x - 1) * (m_heightData.cellInfo.extent.z - 1)));

    uint32 pitch = uint32(m_heightData.cellInfo.extent.x);
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < m_heightData.cellInfo.extent.z - 1; z++)
    {
        for (uint32 x = 0; x < m_heightData.cellInfo.extent.x - 1; x++)
        {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i1;
            indices[i++] = i2;
            indices[i++] = i0;
            indices[i++] = i3;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

static NoiseCombinator& GetTerrainNoiseCombinator()
{
    static struct TerrainNoiseCombinatorInitializer
    {
        NoiseCombinator noiseCombinator;
        TerrainNoiseCombinatorInitializer()
        {
            noiseCombinator.Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, MountainHeight, 0.0f, Vector3(0.35f, 0.35f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vector3(50.0f, 50.0f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, BaseHeight, 0.0f, Vector3(100.0f, 100.0f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.5f, 0.0f, Vector3(50.0f, 50.0f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.25f, 0.0f, Vector3(25.0f, 25.0f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.125f, 0.0f, Vector3(12.5f, 12.5f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.06f, 0.0f, Vector3(6.25f, 6.25f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.03f, 0.0f, Vector3(3.125f, 3.125f, 0.0f) * NoiseScale)
                .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.015f, 0.0f, Vector3(1.56f, 1.56f, 0.0f) * NoiseScale);
        }
    } s_initializer;

    return s_initializer.noiseCombinator;
}

} // namespace terrain

#pragma region TerrainStreamingCell

TerrainStreamingCell::TerrainStreamingCell()
    : StreamingCell()
{
}

TerrainStreamingCell::TerrainStreamingCell(const StreamingCellInfo& cellInfo, const Handle<Scene>& scene, const Handle<Material>& material)
    : StreamingCell(cellInfo),
      m_scene(scene),
      m_material(material)
{
}

TerrainStreamingCell::~TerrainStreamingCell() = default;

void TerrainStreamingCell::OnStreamStart_Impl()
{
    HYP_SCOPE;

    HYP_LOG(WorldGrid, Verbose, "Generating terrain patch at coord {} with extent {} and scale {} on thread {}", m_cellInfo.coord, m_cellInfo.extent, m_cellInfo.scale, CurrentThreadId().GetName());

    terrain::TerrainMeshBuilder meshBuilder(m_cellInfo);
    meshBuilder.GenerateHeights(terrain::GetTerrainNoiseCombinator());

    m_mesh = meshBuilder.BuildMesh();
    m_mesh->Rename(NAME_FMT("Terrain_{}", m_cellInfo.coord));

    g_assetManager->GetAssetRegistry()->RegisterAsset("$Temp/Media/Meshes", m_mesh, AddAssetConflictMode::ReplaceExisting);

    m_mesh->Rename(NAME_FMT("Terrain_{}_Mesh", m_cellInfo.coord));

    InitObject(m_mesh);
}

void TerrainStreamingCell::OnLoaded_Impl()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Assert(m_scene.IsValid(), "Invalid scene!");
    Assert(m_mesh.IsValid(), "Invalid mesh!");
    Assert(m_material.IsValid(), "Invalid material!");

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();
    Assert(entityManager != nullptr);

    HYP_LOG(WorldGrid, Verbose, "Creating terrain patch at coord {} with extent {} and scale {}, bounds: {}\tMesh Id: #{}", m_cellInfo.coord, m_cellInfo.extent, m_cellInfo.scale, m_cellInfo.bounds, m_mesh.Id().Value());

    Transform transform;
    transform.SetTranslation(m_cellInfo.bounds.min);
    transform.SetScale(m_cellInfo.scale);

    Handle<Entity> entity = entityManager->AddEntity();
    entity->SetLocalBounds(m_mesh->GetAABB());

    entityManager->GetComponent<TransformComponent>(entity) = TransformComponent {
        .translation = transform.GetTranslation(),
        .rotation = transform.GetRotation(),
        .scale = transform.GetScale()
    };

    entityManager->GetComponent<VisibilityStateComponent>(entity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

    MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(entity);

    if (meshComponent)
    {
        meshComponent->mesh = m_mesh;
        meshComponent->material = m_material;
    }
    else
    {
        // Add MeshComponent to patch entity
        entityManager->AddComponent<MeshComponent>(entity, MeshComponent { m_mesh, m_material });
    }

    entityManager->AddTag<EntityTag::UpdateRenderProxy>(entity);

    m_node = m_scene->GetRoot()->AddChild();
    m_node->SetName(NAME_FMT("TerrainPatch_{}", m_cellInfo.coord));
    m_node->AddChild(entity);
    m_node->SetLocalTransform(transform);

    // auto result = AssetManager::GetInstance()->Load<Node>("models/sphere16.obj");
    // Assert(result.HasValue());

    // m_node = m_scene->GetRoot()->AddChild();
    // m_node->AddChild(result.GetValue().Result()->GetChild(0));
    // // m_node->Scale(30.0f);
    // m_node->SetWorldTranslation(transform.GetTranslation());
}

void TerrainStreamingCell::OnRemoved_Impl()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_node.IsValid())
    {
        m_node->Remove();
        m_node.Reset();
    }
}

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
