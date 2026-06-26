/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Vertex.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Util/NoiseFactory.hpp>

#include <TerrainStreamingCell.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static constexpr float BaseHeight = 6.0f;
static constexpr float MountainHeight = 65.0f;
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
    static constexpr float ErosionScale = 0.08f;
    static constexpr float Evaporation = 0.9f;
    static constexpr float Erosion = 0.008f * ErosionScale;
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

TerrainStreamingCell::TerrainStreamingCell(
    const StreamingCellInfo& cellInfo,
    const Handle<Scene>& scene,
    const Handle<Mesh>& mesh,
    const Handle<MaterialInstance>& material)
    : StreamingCell(cellInfo),
      m_scene(scene),
      m_mesh(mesh),
      m_material(material)
{
}

TerrainStreamingCell::~TerrainStreamingCell() = default;

void TerrainStreamingCell::OnStreamStart_Impl()
{
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
    entity->SetIsStatic(true);

    entityManager->GetComponent<TransformComponent>(entity) = TransformComponent {
        .translation = transform.GetTranslation(),
        .rotation = transform.GetRotation(),
        .scale = transform.GetScale()
    };

    entityManager->GetComponent<VisibilityStateComponent>(entity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

    MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(entity);

    if (!meshComponent)
    {
        meshComponent = &entityManager->AddComponent<MeshComponent>(entity, MeshComponent { m_mesh, m_material });
    }
    else
    {
        meshComponent->mesh = m_mesh;
        meshComponent->material = m_material;
    }
    
    // terrain cells share a mesh, we can use instancing for them
    meshComponent->enableAutoInstancing = true;

    entityManager->AddTag<EntityTag::UpdateRenderProxy>(entity);

    m_node = m_scene->GetRoot()->AddChild();
    m_node->SetName(NAME_FMT("TerrainPatch_{}", m_cellInfo.coord));
    m_node->AddChild(entity);
    m_node->SetLocalTransform(transform);
    m_node->SetIsStatic(true);

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
        m_node->Remove(/* moveToDetached */ false);
        m_node.Reset();
    }
}

#pragma endregion TerrainStreamingCell

} // namespace Hyperion
