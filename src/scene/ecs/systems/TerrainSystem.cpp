#include <scene/ecs/systems/TerrainSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

static const float base_height = 2.0f;
static const float mountain_height = 35.0f;
static const float global_terrain_noise_scale = 1.0f;

static const FixedArray<TerrainPatchNeighbor, 8> GetPatchNeighbors(const TerrainPatchCoord &coord)
{
    return {
        TerrainPatchNeighbor { coord + TerrainPatchCoord {  1,  0 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord { -1,  0 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord {  0,  1 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord {  0, -1 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord {  1, -1 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord { -1, -1 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord {  1,  1 } },
        TerrainPatchNeighbor { coord + TerrainPatchCoord { -1,  1 } }
    };
}

static TerrainPatchCoord WorldSpaceToPatchCoord(const Vec3f &world_position, const TerrainComponent &terrain_component, const TransformComponent &transform_component)
{
    Vec3f scaled = world_position - transform_component.transform.GetTranslation();
    scaled *= Vec3f::One() / (terrain_component.scale * (Vec3f(terrain_component.patch_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return TerrainPatchCoord { int(scaled.x), int(scaled.z) };
}

namespace terrain {

struct TerrainHeight
{
    float height;
    float erosion;
    float sediment;
    float water;
    float new_water;
    float displacement;
};

struct TerrainHeightData
{
    TerrainPatchInfo        patch_info;
    Array<TerrainHeight>    heights;

    TerrainHeightData(const TerrainPatchInfo &patch_info)
        : patch_info(patch_info)
    {
        heights.Resize(patch_info.extent.width * patch_info.extent.depth);
    }

    TerrainHeightData(const TerrainHeightData &other)                   = delete;
    TerrainHeightData &operator=(const TerrainHeightData &other)        = delete;
    TerrainHeightData(TerrainHeightData &&other) noexcept               = delete;
    TerrainHeightData &operator=(TerrainHeightData &&other) noexcept    = delete;
    ~TerrainHeightData()                                                = default;

    uint GetHeightIndex(int x, int z) const
    {
        return uint(((x + int(patch_info.extent.width)) % int(patch_info.extent.width))
                + ((z + int(patch_info.extent.depth)) % int(patch_info.extent.depth)) * int(patch_info.extent.width));
    }
};

class TerrainErosion
{
    static constexpr uint num_iterations = 250u;
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
    for (uint iteration = 0; iteration < num_iterations; iteration++) {
        for (int z = 1; z < height_data.patch_info.extent.depth - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.width - 2; x++) {
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

        for (int z = 1; z < height_data.patch_info.extent.depth - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.width - 2; x++) {
                auto &height_info = height_data.heights[height_data.GetHeightIndex(x, z)];

                height_info.water += height_info.new_water;
                height_info.new_water = 0.0f;

                const auto old_height = height_info.height;
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
    TerrainMeshBuilder(const TerrainPatchInfo &patch_info);
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

TerrainMeshBuilder::TerrainMeshBuilder(const TerrainPatchInfo &patch_info)
    : m_height_data(patch_info)
{
}

void TerrainMeshBuilder::GenerateHeights(const NoiseCombinator &noise_combinator)
{
    Threads::AssertOnThread(THREAD_TASK);

    DebugLog(
        LogType::Debug,
        "Generate Terrain mesh at coord [%d, %d]\n",
        m_height_data.patch_info.coord.x,
        m_height_data.patch_info.coord.y
    );

    for (int z = 0; z < int(m_height_data.patch_info.extent.depth); z++) {
        for (int x = 0; x < int(m_height_data.patch_info.extent.width); x++) {
            const float x_offset = float(x + (m_height_data.patch_info.coord.x * int(m_height_data.patch_info.extent.width - 1))) / float(m_height_data.patch_info.extent.width);
            const float z_offset = float(z + (m_height_data.patch_info.coord.y * int(m_height_data.patch_info.extent.depth - 1))) / float(m_height_data.patch_info.extent.depth);

            const uint index = m_height_data.GetHeightIndex(x, z);

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
    Threads::AssertOnThread(THREAD_TASK);

    Array<Vertex> vertices = BuildVertices();
    Array<Mesh::Index> indices = BuildIndices();

    auto mesh = CreateObject<Mesh>(
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
    vertices.Resize(m_height_data.patch_info.extent.width * m_height_data.patch_info.extent.depth);

    uint i = 0;

    for (uint z = 0; z < m_height_data.patch_info.extent.depth; z++) {
        for (uint x = 0; x < m_height_data.patch_info.extent.width; x++) {
            Vec3f position(x, m_height_data.heights[i].height, z);
            position *= m_height_data.patch_info.scale;

            Vec2f texcoord(
               float(x) / float(m_height_data.patch_info.extent.width),
               float(z) / float(m_height_data.patch_info.extent.depth)
            );

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

Array<uint32> TerrainMeshBuilder::BuildIndices() const
{
    Array<uint32> indices;
    indices.Resize(6 * (m_height_data.patch_info.extent.width - 1) * (m_height_data.patch_info.extent.depth - 1));

    uint pitch = m_height_data.patch_info.extent.width;
    uint row = 0;

    uint i0 = row;
    uint i1 = row + 1;
    uint i2 = pitch + i1;
    uint i3 = pitch + row;

    uint i = 0;

    for (uint z = 0; z < m_height_data.patch_info.extent.depth - 1; z++) {
        for (uint x = 0; x < m_height_data.patch_info.extent.width - 1; x++) {
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

void TerrainSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, terrain_component, transform_component, mesh_component] : entity_manager.GetEntitySet<TerrainComponent, TransformComponent, MeshComponent>()) {
        if (!(terrain_component.flags & TERRAIN_COMPONENT_FLAG_INIT)) {
            mesh_component.material = CreateObject<Material>(HYP_NAME(terrain_material));
            mesh_component.material->SetBucket(BUCKET_OPAQUE);
            mesh_component.material->SetIsDepthTestEnabled(true);
            mesh_component.material->SetIsDepthWriteEnabled(true);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.85f);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 1.0f);

            if (auto albedo_texture = g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-albedo.png")) {
                albedo_texture->GetImage()->SetIsSRGB(true);
                mesh_component.material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(albedo_texture));
            }

            mesh_component.material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-preview.png"));

            InitObject(mesh_component.material);

            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;

            terrain_component.flags |= TERRAIN_COMPONENT_FLAG_INIT;

            DebugLog(
                LogType::Info,
                "Terrain entity [%u] initialized\n",
                entity_id.Value()
            );
        }

        RC<TerrainGenerationState> &state = m_states[entity_id];

        if (!state) {
            state.Reset(new TerrainGenerationState());

            state->patch_generation_queue_shared.Reset(new TerrainGenerationQueue());

            state->noise_combinator.Reset(new NoiseCombinator(terrain_component.seed));
            (*state->noise_combinator)
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

        if (state->patch_generation_queue_shared->has_updates.Get(MemoryOrder::RELAXED)) {
            state->patch_generation_queue_shared->mutex.Lock();

            state->patch_generation_queue_shared->has_updates.Set(false, MemoryOrder::RELAXED);
            state->patch_generation_queue_owned = std::move(state->patch_generation_queue_shared->queue);

            state->patch_generation_queue_shared->mutex.Unlock();

            while (state->patch_generation_queue_owned.Any()) {
                TerrainGenerationResult result = state->patch_generation_queue_owned.Pop();
                const TerrainPatchInfo &patch_info = result.patch_info;

                const auto patch_generation_task_it = state->patch_generation_tasks.Find(patch_info.coord);

                if (patch_generation_task_it == state->patch_generation_tasks.End()) {
                    DebugLog(
                        LogType::Info,
                        "Generation task for patch coord [%d, %d] no longer in map, must have been removed. Skipping.\n",
                        patch_info.coord.x,
                        patch_info.coord.y
                    );

                    continue;
                }

                state->patch_generation_tasks.Erase(patch_generation_task_it);

                DebugLog(
                    LogType::Debug,
                    "Add completed terrain mesh at coord [%d, %d]\n",
                    patch_info.coord.x,
                    patch_info.coord.y
                );

                Handle<Mesh> mesh = std::move(result.mesh);
                AssertThrowMsg(mesh.IsValid(), "Terrain mesh is invalid");
                InitObject(mesh);

                entity_manager.PushCommand([state, coord = patch_info.coord, mesh = std::move(mesh), material = mesh_component.material](EntityManager &mgr, GameCounter::TickUnit delta) mutable
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when updating mesh\n",
                            coord.x, coord.y
                        );

                        return;
                    }

                    // Prevent patch entity from being removed while updating
                    Mutex::Guard guard(state->patch_entities_mutex);

                    MeshComponent *patch_mesh_component = mgr.TryGetComponent<MeshComponent>(patch_entity);
                    BoundingBoxComponent *patch_bounding_box_component = mgr.TryGetComponent<BoundingBoxComponent>(patch_entity);
                    TransformComponent *patch_transform_component = mgr.TryGetComponent<TransformComponent>(patch_entity);

                    if (patch_bounding_box_component) {
                        patch_bounding_box_component->local_aabb = mesh->GetAABB();

                        if (patch_transform_component) {
                            patch_bounding_box_component->world_aabb = BoundingBox::empty;

                            for (const Vec3f &corner : patch_bounding_box_component->local_aabb.GetCorners()) {
                                patch_bounding_box_component->world_aabb.Extend(patch_transform_component->transform.GetMatrix() * corner);
                            }
                        }
                    }

                    if (patch_mesh_component) {
                        patch_mesh_component->mesh = std::move(mesh);
                        patch_mesh_component->material = std::move(material);
                        patch_mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
                    } else {
                        // Add MeshComponent to patch entity
                        mgr.AddComponent<MeshComponent>(patch_entity, MeshComponent {
                            .mesh       = std::move(mesh),
                            .material   = std::move(material)
                        });
                    }
                });
            }
        }

        const TerrainPatchCoord camera_patch_coord = WorldSpaceToPatchCoord(terrain_component.camera_position, terrain_component, transform_component);

        if (!state->GetPatchEntity(camera_patch_coord).IsValid()) {
            // Enqueue a patch to be created at the current camera position
            if (!state->queued_neighbors.Contains(camera_patch_coord)) {
                state->patch_update_queue.Push({
                    .coord  = camera_patch_coord,
                    .state  = TerrainPatchState::WAITING
                });

                state->queued_neighbors.Insert(camera_patch_coord);
            }
        }

        Array<TerrainPatchCoord> patch_coords_in_range;

        for (int x = MathUtil::Floor(-terrain_component.max_distance); x <= MathUtil::Ceil(terrain_component.max_distance) + 1; ++x) {
            for (int z = MathUtil::Floor(-terrain_component.max_distance); z <= MathUtil::Ceil(terrain_component.max_distance) + 1; ++z) {
                patch_coords_in_range.PushBack(camera_patch_coord + TerrainPatchCoord { x, z });
            }
        }

        Array<TerrainPatchCoord> patch_coords_to_add(patch_coords_in_range);

        // Handle patch states
        while (state->patch_update_queue.Any()) {
            const TerrainPatchUpdate update = state->patch_update_queue.Pop();

            switch (update.state) {
            case TerrainPatchState::WAITING: {
                //AddPatch(update.coord);

                DebugLog(LogType::Debug, "Add patch at [%d, %d]\n", update.coord.x, update.coord.y);

                const TerrainPatchInfo patch_info {
                    .extent     = terrain_component.patch_size,
                    .coord      = update.coord,
                    .scale      = terrain_component.scale,
                    .state      = TerrainPatchState::LOADED,
                    .neighbors  = GetPatchNeighbors(update.coord)
                };

                // add command to add the entity
                entity_manager.PushCommand([state, patch_info, translation = transform_component.transform.GetTranslation()](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = mgr.AddEntity();

                    // Add TerrainPatchComponent
                    mgr.AddComponent<TerrainPatchComponent>(patch_entity, TerrainPatchComponent {
                        .patch_info = patch_info
                    });

                    // Add TransformComponent
                    mgr.AddComponent<TransformComponent>(patch_entity, TransformComponent {
                        .transform = Transform {
                            Vec3f {
                                translation.x + (float(patch_info.coord.x) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.x,
                                translation.y,
                                translation.z + (float(patch_info.coord.y) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.z
                            }
                        }
                    });

                    // Add VisibilityStateComponent
                    mgr.AddComponent<VisibilityStateComponent>(patch_entity, VisibilityStateComponent { });

                    // Add BoundingBoxComponent
                    mgr.AddComponent<BoundingBoxComponent>(patch_entity, BoundingBoxComponent { });

                    DebugLog(LogType::Debug, "Patch entity at [%d, %d] added\n", patch_info.coord.x, patch_info.coord.y);

                    state->AddPatchEntity(patch_entity, patch_info.coord);
                });

                // add task to generation queue
                const TaskRef generation_task_ref = TaskSystem::GetInstance().ScheduleTask([patch_info, generation_queue = state->patch_generation_queue_shared, noise_combinator = state->noise_combinator]()
                {
                    terrain::TerrainMeshBuilder mesh_builder(patch_info);
                    mesh_builder.GenerateHeights(*noise_combinator);

                    Handle<Mesh> mesh = mesh_builder.BuildMesh();
                    AssertThrowMsg(mesh.IsValid(), "Generated terrain mesh is invalid");

                    DebugLog(LogType::Debug, "From thread: %s\tTerrain mesh has %u indices\n", Threads::CurrentThreadID().name.LookupString(), mesh->NumIndices());

                    Mutex::Guard guard(generation_queue->mutex);

                    generation_queue->queue.Push(TerrainGenerationResult {
                        .patch_info = patch_info,
                        .mesh       = std::move(mesh)
                    });

                    generation_queue->has_updates.Set(true, MemoryOrder::RELAXED);

                    DebugLog(
                        LogType::Debug,
                        "Terrain mesh at coord [%d, %d] generation completed\n",
                        patch_info.coord.x,
                        patch_info.coord.y
                    );
                }, THREAD_POOL_GENERIC);

                state->patch_generation_tasks.Insert(patch_info.coord, generation_task_ref);

                break;
            }
            case TerrainPatchState::UNLOADED: {
                DebugLog(
                    LogType::Debug,
                    "Unload patch at [%d, %d]\n",
                    update.coord.x, update.coord.y
                );

                const auto patch_generation_task_it = state->patch_generation_tasks.Find(update.coord);

                if (patch_generation_task_it != state->patch_generation_tasks.End()) {
                    const TaskRef patch_generation_task = patch_generation_task_it->second;

                    TaskSystem::GetInstance().Unschedule(patch_generation_task);

                    state->patch_generation_tasks.Erase(patch_generation_task_it);
                }

                const auto queued_neighbors_it = state->queued_neighbors.Find(update.coord);

                if (queued_neighbors_it != state->queued_neighbors.End()) {
                    state->queued_neighbors.Erase(queued_neighbors_it);
                }

                // Push command to remove the entity
                entity_manager.PushCommand([state, update](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(update.coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when unloading\n",
                            update.coord.x, update.coord.y
                        );

                        return;
                    }

                    AssertThrowMsg(
                        state->RemovePatchEntity(patch_entity),
                        "Failed to remove patch entity at [%d, %d]",
                        update.coord.x,
                        update.coord.y
                    );

                    if (mgr.HasEntity(patch_entity)) {
                        mgr.RemoveEntity(patch_entity);
                    }

                    DebugLog(
                        LogType::Debug,
                        "Patch entity at [%d, %d] removed\n",
                        update.coord.x, update.coord.y
                    );
                });

                break;
            }
            default: {
                // Push command to update patch state
                entity_manager.PushCommand([state, update](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(update.coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when updating state\n",
                            update.coord.x, update.coord.y
                        );

                        return;
                    }

                    TerrainPatchComponent *patch_component = mgr.TryGetComponent<TerrainPatchComponent>(patch_entity);
                    AssertThrowMsg(patch_component, "Patch entity did not have a TerrainPatchComponent when updating state");

                    // set new state
                    patch_component->patch_info.state = update.state;
                });

                break;
            }
            }
        }

        {
            Mutex::Guard guard(state->patch_entities_mutex);

            for (auto &it : state->patch_entities) {
                const auto in_range_it = patch_coords_in_range.Find(it.first);

                const bool is_in_range = in_range_it != patch_coords_in_range.End();

                if (is_in_range) {
                    patch_coords_to_add.Erase(it.first);
                }

                // Push command to update patch state
                // @FIXME: thread safety issues with using state here
                entity_manager.PushCommand([patch_coord = it.first, is_in_range, state](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> entity = state->GetPatchEntity(patch_coord);

                    if (!entity.IsValid()) {
                        // Patch entity was removed, skip
                        return;
                    }

                    TerrainPatchComponent *terrian_patch_component = mgr.TryGetComponent<TerrainPatchComponent>(entity);

                    AssertThrowMsg(
                        terrian_patch_component,
                        "Patch entity at [%d, %d] did not have a TerrainPatchComponent when updating state",
                        patch_coord.x,
                        patch_coord.y
                    );

                    const TerrainPatchInfo &patch_info = terrian_patch_component->patch_info;

                    switch (patch_info.state) {
                    case TerrainPatchState::LOADED: {
                        // reset unload timer
                        terrian_patch_component->patch_info.unload_timer = 0.0f;

                        auto queued_neighbors_it = state->queued_neighbors.Find(patch_info.coord);

                        // Remove loaded patch from queued neighbors
                        if (queued_neighbors_it != state->queued_neighbors.End()) {
                            state->queued_neighbors.Erase(queued_neighbors_it);
                        }

                        if (!is_in_range) {
                            DebugLog(
                                LogType::Debug,
                                "Patch [%d, %d] no longer in range, unloading\n",
                                patch_info.coord.x, patch_info.coord.y
                            );

                            // Start unloading
                            state->patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = TerrainPatchState::UNLOADING
                            });
                        }

                        break;
                    }
                    case TerrainPatchState::UNLOADING:
                        if (is_in_range) {
                            DebugLog(
                                LogType::Debug,
                                "Patch [%d, %d] back in range, stopping unloading\n",
                                patch_info.coord.x, patch_info.coord.y
                            );

                            // Stop unloading
                            state->patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = TerrainPatchState::LOADED
                            });
                        } else {
                            terrian_patch_component->patch_info.unload_timer += delta;

                            if (terrian_patch_component->patch_info.unload_timer >= 10.0f) {
                                DebugLog(
                                    LogType::Debug,
                                    "Unloading patch at [%d, %d]\n",
                                    patch_info.coord.x, patch_info.coord.y
                                );

                                // Unload it now
                                state->patch_update_queue.Push({
                                    .coord  = patch_info.coord,
                                    .state  = TerrainPatchState::UNLOADED
                                });
                            }
                        }

                        break;
                    default:
                        break;
                    }
                });
            }
        }

        // enqueue a patch to be created for each patch in range
        for (const TerrainPatchCoord &coord : patch_coords_to_add) {
            if (!state->queued_neighbors.Contains(coord)) {
                state->patch_update_queue.Push({
                    .coord  = coord,
                    .state  = TerrainPatchState::WAITING
                });

                state->queued_neighbors.Insert(coord);
            }
        }
    }
}

} // namespace hyperion::v2
