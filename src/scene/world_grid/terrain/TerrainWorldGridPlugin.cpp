/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/Texture.hpp>

#include <core/math/Vertex.hpp>

#include <asset/Assets.hpp>

#include <core/logging/Logger.hpp>

#include <util/NoiseFactory.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(WorldGrid);
HYP_DEFINE_LOG_SUBCHANNEL(TerrainWorldGridPlugin, WorldGrid);

static const float base_height = 2.0f;
static const float mountain_height = 35.0f;
static const float global_terrain_noise_scale = 1.0f;

#pragma region TerrainWorldGridPatch

class TerrainWorldGridPatch : public WorldGridPatch
{
public:
    TerrainWorldGridPatch(const WorldGridPatchInfo &patch_info, const Handle<Mesh> &mesh, const Handle<Material> &material)
        : WorldGridPatch(patch_info),
          m_mesh(mesh),
          m_material(material)
    {
    }

    virtual ~TerrainWorldGridPatch() override
    {
    }

    virtual void InitializeEntity(const Handle<Scene> &scene, ID<Entity> entity) override
    {
        HYP_SCOPE;

        if (!m_mesh.IsValid()) {
            HYP_LOG(TerrainWorldGridPlugin, Error, "Terrain mesh is invalid");

            return;
        }

        const RC<EntityManager> &entity_manager = scene->GetEntityManager();
        AssertThrow(entity_manager != nullptr);

        MeshComponent *patch_mesh_component = entity_manager->TryGetComponent<MeshComponent>(entity);
        BoundingBoxComponent *patch_bounding_box_component = entity_manager->TryGetComponent<BoundingBoxComponent>(entity);
        TransformComponent *patch_transform_component = entity_manager->TryGetComponent<TransformComponent>(entity);

        if (patch_bounding_box_component) {
            patch_bounding_box_component->local_aabb = m_mesh->GetAABB();

            if (patch_transform_component) {
                patch_bounding_box_component->world_aabb = BoundingBox::Empty();

                for (const Vec3f &corner : patch_bounding_box_component->local_aabb.GetCorners()) {
                    patch_bounding_box_component->world_aabb = patch_bounding_box_component->world_aabb.Union(patch_transform_component->transform.GetMatrix() * corner);
                }
            }
        }

        if (patch_mesh_component) {
            patch_mesh_component->mesh = m_mesh;
            patch_mesh_component->material = m_material;
        } else {
            // Add MeshComponent to patch entity
            entity_manager->AddComponent<MeshComponent>(entity, MeshComponent {
                .mesh       = m_mesh,
                .material   = m_material
            });
        }

        entity_manager->AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }

    virtual void Update(GameCounter::TickUnit delta) override
    {

    }

protected:
    Handle<Mesh>        m_mesh;
    Handle<Material>    m_material;
};

#pragma endregion TerrainWorldGridPatch

namespace terrain {

struct TerrainHeight
{
    float   height;
    float   erosion;
    float   sediment;
    float   water;
    float   new_water;
    float   displacement;
};

struct TerrainHeightData
{
    WorldGridPatchInfo      patch_info;
    Array<TerrainHeight>    heights;

    TerrainHeightData(const WorldGridPatchInfo &patch_info)
        : patch_info(patch_info)
    {
        heights.Resize(patch_info.extent.x * patch_info.extent.z);
    }

    TerrainHeightData(const TerrainHeightData &other)                   = delete;
    TerrainHeightData &operator=(const TerrainHeightData &other)        = delete;
    TerrainHeightData(TerrainHeightData &&other) noexcept               = delete;
    TerrainHeightData &operator=(TerrainHeightData &&other) noexcept    = delete;
    ~TerrainHeightData()                                                = default;

    uint32 GetHeightIndex(int x, int z) const
    {
        return uint32(((x + patch_info.extent.x) % patch_info.extent.x)
            + ((z + patch_info.extent.z) % patch_info.extent.z) * patch_info.extent.x);
    }
};

class TerrainErosion
{
    static constexpr uint32 num_iterations = 250u;
    static constexpr float erosion_scale = 0.05f;
    static constexpr float evaporation = 0.9f;
    static constexpr float erosion = 0.004f * erosion_scale;
    static constexpr float deposition = 0.0000002f * erosion_scale;

    static const FixedArray<Pair<int, int>, 8> offsets;

public:
    static void Erode(TerrainHeightData &height_data);
};

const FixedArray<Pair<int, int>, 8> TerrainErosion::offsets {
    Pair<int, int> { 1, 0 },
    Pair<int, int> { 1, 1 },
    Pair<int, int> { 1, -1 },
    Pair<int, int> { 0, 1 },
    Pair<int, int> { 0, -1 },
    Pair<int, int> { -1, 0 },
    Pair<int, int> { -1, 1 },
    Pair<int, int> { -1, -1 }
};

void TerrainErosion::Erode(TerrainHeightData &height_data)
{
    for (uint32 iteration = 0; iteration < num_iterations; iteration++) {
        for (int z = 1; z < height_data.patch_info.extent.z - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.x - 2; x++) {
                TerrainHeight &height_info = height_data.heights[height_data.GetHeightIndex(x, z)];
                height_info.displacement = 0.0f;

                for (const auto &offset : offsets) {
                    const auto &neighbor_height_info = height_data.heights[height_data.GetHeightIndex(x + offset.first, z + offset.second)];

                    height_info.displacement += MathUtil::Max(height_info.height - neighbor_height_info.height, 0.0f);
                }

                if (height_info.displacement != 0.0f) {
                    float water = height_info.water * evaporation;
                    float staying_water = (water * 0.0002f) / (height_info.displacement * erosion_scale + 1);
                    water -= staying_water;

                    for (const auto &offset : offsets) {
                        auto &neighbor_height_info = height_data.heights[height_data.GetHeightIndex(x + offset.first, z + offset.second)];

                        neighbor_height_info.new_water += MathUtil::Max(height_info.height - neighbor_height_info.height, 0.0f) / height_info.displacement * water;
                    }

                    height_info.water = staying_water + 1.0f;
                }

            }
        }

        for (int z = 1; z < height_data.patch_info.extent.z - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.x - 2; x++) {
                TerrainHeight &height_info = height_data.heights[height_data.GetHeightIndex(x, z)];

                height_info.water += height_info.new_water;
                height_info.new_water = 0.0f;

                const float old_height = height_info.height;
                height_info.height += (-(height_info.displacement - (0.005f / erosion_scale)) * height_info.water) * erosion + height_info.water * deposition;
                height_info.erosion = old_height - height_info.height;

                if (old_height < height_info.height) {
                    height_info.water = MathUtil::Max(height_info.water - (height_info.height - old_height) * 1000.0f, 0.0f);
                }
            }
        }
    }
}

class TerrainMeshBuilder
{
public:
    TerrainMeshBuilder(const WorldGridPatchInfo &patch_info);
    TerrainMeshBuilder(const TerrainMeshBuilder &other)                 = delete;
    TerrainMeshBuilder &operator=(const TerrainMeshBuilder &other)      = delete;
    TerrainMeshBuilder(TerrainMeshBuilder &&other) noexcept             = delete;
    TerrainMeshBuilder &operator=(TerrainMeshBuilder &&other) noexcept  = delete;
    ~TerrainMeshBuilder()                                               = default;

    void GenerateHeights(const NoiseCombinator &noise_combinator);
    Handle<Mesh> BuildMesh() const;

private:
    Array<Vertex> BuildVertices() const;
    Array<Mesh::Index> BuildIndices() const;

    TerrainHeightData m_height_data;
};

TerrainMeshBuilder::TerrainMeshBuilder(const WorldGridPatchInfo &patch_info)
    : m_height_data(patch_info)
{
}

void TerrainMeshBuilder::GenerateHeights(const NoiseCombinator &noise_combinator)
{
    Threads::AssertOnThread(ThreadCategory::THREAD_CATEGORY_TASK);

    for (int z = 0; z < int(m_height_data.patch_info.extent.z); z++) {
        for (int x = 0; x < int(m_height_data.patch_info.extent.x); x++) {
            const float x_offset = float(x + (m_height_data.patch_info.coord.x * int(m_height_data.patch_info.extent.x - 1))) / float(m_height_data.patch_info.extent.x);
            const float z_offset = float(z + (m_height_data.patch_info.coord.y * int(m_height_data.patch_info.extent.z - 1))) / float(m_height_data.patch_info.extent.z);

            const uint32 index = m_height_data.GetHeightIndex(x, z);

            m_height_data.heights[index] = TerrainHeight {
                .height         = noise_combinator.GetNoise(Vec2f(x_offset, z_offset)),
                .erosion        = 0.0f,
                .sediment       = 0.0f,
                .water          = 1.0f,
                .new_water      = 0.0f,
                .displacement   = 0.0f
            };
        }
    }

    // TerrainErosion::Erode(m_height_data);
}

Handle<Mesh> TerrainMeshBuilder::BuildMesh() const
{
    Threads::AssertOnThread(ThreadCategory::THREAD_CATEGORY_TASK);

    Array<Vertex> vertices = BuildVertices();
    Array<Mesh::Index> indices = BuildIndices();

    Handle<Mesh> mesh = CreateObject<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        static_mesh_vertex_attributes
    );

    mesh->CalculateNormals();
    mesh->CalculateTangents();

    return mesh;
}

Array<Vertex> TerrainMeshBuilder::BuildVertices() const
{
    Array<Vertex> vertices;
    vertices.Resize(m_height_data.patch_info.extent.x * m_height_data.patch_info.extent.z);

    int i = 0;

    for (int z = 0; z < m_height_data.patch_info.extent.z; z++) {
        for (int x = 0; x < m_height_data.patch_info.extent.x; x++) {
            const Vec3f position = Vec3f { float(x), m_height_data.heights[i].height, float(z) } * m_height_data.patch_info.scale;

            const Vec2f texcoord(
               float(x) / float(m_height_data.patch_info.extent.x),
               float(z) / float(m_height_data.patch_info.extent.z)
            );

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

Array<uint32> TerrainMeshBuilder::BuildIndices() const
{
    Array<uint32> indices;
    indices.Resize(SizeType(6 * (m_height_data.patch_info.extent.x - 1) * (m_height_data.patch_info.extent.z - 1)));

    uint32 pitch = uint32(m_height_data.patch_info.extent.x);
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < m_height_data.patch_info.extent.z - 1; z++) {
        for (uint32 x = 0; x < m_height_data.patch_info.extent.x - 1; x++) {
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

} // namespace terrain

TerrainWorldGridPlugin::TerrainWorldGridPlugin()
    : m_noise_combinator(MakeUnique<NoiseCombinator>(0x123F/* TODO: seed */))
{
    (*m_noise_combinator)
        .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, mountain_height, 0.0f, Vector3(0.35f, 0.35f, 0.0f) * global_terrain_noise_scale)
        // .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, base_height, 0.0f, Vector3(100.0f, 100.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, base_height * 0.5f, 0.0f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, base_height * 0.25f, 0.0f, Vector3(25.0f, 25.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, base_height * 0.125f, 0.0f, Vector3(12.5f, 12.5f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, base_height * 0.06f, 0.0f, Vector3(6.25f, 6.25f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, base_height * 0.03f, 0.0f, Vector3(3.125f, 3.125f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, base_height * 0.015f, 0.0f, Vector3(1.56f, 1.56f, 0.0f) * global_terrain_noise_scale);
}

TerrainWorldGridPlugin::~TerrainWorldGridPlugin()
{
}

void TerrainWorldGridPlugin::Initialize()
{
    HYP_SCOPE;

    HYP_LOG(TerrainWorldGridPlugin, Debug, "Initializing TerrainWorldGridPlugin");

    Threads::AssertOnThread(g_game_thread);

    m_material = CreateObject<Material>(NAME("terrain_material"));
    m_material->SetBucket(BUCKET_OPAQUE);
    m_material->SetIsDepthTestEnabled(true);
    m_material->SetIsDepthWriteEnabled(true);
    m_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.85f);
    m_material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
    m_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, Vec2f(10.0f));

    if (auto albedo_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-albedo.png")) {
        Handle<Texture> albedo_texture = albedo_texture_asset->Result();
        albedo_texture->GetImage()->SetIsSRGB(true);
        
        m_material->SetTexture(MaterialTextureKey::ALBEDO_MAP, albedo_texture);
    }

    if (auto ground_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-preview.png")) {
        m_material->SetTexture(MaterialTextureKey::NORMAL_MAP, ground_texture_asset->Result());
    }

    InitObject(m_material);
}

void TerrainWorldGridPlugin::Shutdown()
{
    HYP_SCOPE;

    HYP_LOG(TerrainWorldGridPlugin, Info, "Shutting down TerrainWorldGridPlugin");
}

void TerrainWorldGridPlugin::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
}

UniquePtr<WorldGridPatch> TerrainWorldGridPlugin::CreatePatch(const WorldGridPatchInfo &patch_info)
{
    HYP_SCOPE;

    terrain::TerrainMeshBuilder mesh_builder(patch_info);
    mesh_builder.GenerateHeights(*m_noise_combinator);

    Handle<Mesh> mesh = mesh_builder.BuildMesh();
    AssertThrowMsg(mesh.IsValid(), "Generated terrain mesh is invalid");

    InitObject(mesh);

    return MakeUnique<TerrainWorldGridPatch>(patch_info, mesh, m_material);
}

} // namespace hyperion
