#include "TerrainPagingController.hpp"
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <rendering/Texture.hpp>
#include <util/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

TerrainPagingController::TerrainPagingController()
    : PagingController(Extent3D { 64, 64, 64 }, Vector3::one, 3.0f),
      m_noise_combinator(0x12345),
      m_seed(0x12345)
{
}

TerrainPagingController::TerrainPagingController(
    Seed seed,
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : PagingController(patch_size, scale, max_distance),
    m_noise_combinator(seed),
    m_seed(seed)
{
}

void TerrainPagingController::OnAdded()
{
    constexpr Float base_height = 20.0f;
    constexpr Float mountain_height = 350.0f;
    constexpr Float global_terrain_noise_scale = 1.0f;

    m_noise_combinator
        .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, mountain_height, 0.0f, Vector3(0.35f, 0.35f, 0.0f) * global_terrain_noise_scale)
        // .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, base_height, 0.0f, Vector3(100.0f, 100.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, base_height * 0.5f, 0.0f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, base_height * 0.25f, 0.0f, Vector3(25.0f, 25.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, base_height * 0.125f, 0.0f, Vector3(12.5f, 12.5f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, base_height * 0.06f, 0.0f, Vector3(6.25f, 6.25f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, base_height * 0.03f, 0.0f, Vector3(3.125f, 3.125f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, base_height * 0.015f, 0.0f, Vector3(1.56f, 1.56f, 0.0f) * global_terrain_noise_scale);

    m_material = CreateObject<Material>(HYP_NAME(terrain_material));

    // m_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.2f, 0.99f, 0.5f, 1.0f));
    m_material->SetBucket(BUCKET_OPAQUE);
    m_material->SetIsDepthTestEnabled(true);
    m_material->SetIsDepthWriteEnabled(true);
    m_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.85f);
    m_material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
    m_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 3.0f);

    if (auto albedo_texture = g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-albedo.png")) {
        albedo_texture->GetImage()->SetIsSRGB(true);
        m_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(albedo_texture));
    }

    m_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-preview.png"));

    InitObject(m_material);

    PagingController::OnAdded();
}

void TerrainPagingController::OnRemoved()
{
    // remove all tasks
    for (const auto &enqueued_patch : m_enqueued_patches) {
        const auto &task = enqueued_patch.second;

        TaskSystem::GetInstance().Unschedule(task);
    }

    PagingController::OnRemoved();
}

void TerrainPagingController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_terrain_generation_flag.load()) {
        AddEnqueuedChunks();
    }

    PagingController::OnUpdate(delta);

    ++m_update_log_timer;

    if (m_update_log_timer >= 1000) {
        DebugLog(
            LogType::Debug,
            "Currently have %u terrain chunks\n",
            static_cast<UInt>(m_patches.Size())
        );
        
        m_update_log_timer = 0;
    }
}

void TerrainPagingController::OnPatchAdded(Patch *patch)
{
    // just in case a patch was quickly added, enqueued, removed, then added back again.
    if (m_enqueued_patches.Contains(patch->info.coord)) {
        DebugLog(
            LogType::Info,
            "Terrain patch at [%f, %f] already enqueued for generation, skipping.\n",
            patch->info.coord.x,
            patch->info.coord.y
        );

        return;
    }

    DebugLog(LogType::Info, "Terrain patch added at [%f, %f], enqueuing terrain generation\n", patch->info.coord.x, patch->info.coord.y);

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;
    
    Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(Terrain), ShaderProperties(vertex_attributes));
    AssertThrow(shader.IsValid());

    // @TODO new ECS

    // patch->entity = CreateObject<Entity>(
    //     Handle<Mesh>(), // mesh added later, after task thread generates it
    //     Handle<Shader>(shader),
    //     Handle<Material>(m_material),
    //     RenderableAttributeSet(
    //         MeshAttributes {
    //             .vertex_attributes = vertex_attributes
    //         },
    //         MaterialAttributes {
    //             .shader_definition = shader->GetCompiledShader().GetDefinition(),
    //             .bucket = Bucket::BUCKET_OPAQUE,
    //             .blend_mode = BlendMode::NONE,
    //             .cull_faces = FaceCullMode::NONE
    //         }
    //     )
    // );

    // patch->entity->SetName(CreateNameFromDynamicString(
    //     ANSIString("terrain_chunk_")
    //     + ANSIString::ToString(Int(patch->info.coord.x))
    //     + "_" + ANSIString::ToString(Int(patch->info.coord.y))
    // ));

    // patch->entity->SetTranslation({
    //     (patch->info.coord.x - 0.5f) * (Vector3(patch->info.extent).Max() - 1) * m_scale.x,
    //     GetOwner()->GetTranslation().y,
    //     (patch->info.coord.y - 0.5f) * (Vector3(patch->info.extent).Max() - 1) * m_scale.z
    // });

    // for (const ID<Scene> &id : GetOwner()->GetScenes()) {
    //     if (auto scene = Handle<Scene>(id)) {
    //         // @FIXME new ECS
    //         // scene->AddEntity(Handle<Entity>(patch->entity));
    //     }
    // }

    const auto task_ref = TaskSystem::GetInstance().ScheduleTask([this, patch_info = patch->info]() {
        TerrainMeshBuilder builder(patch_info);
        builder.GenerateHeights(m_noise_combinator);

        auto mesh = builder.BuildMesh();

        m_terrain_generation_sp.Wait();

        m_shared_terrain_mesh_queue.Push(TerrainGenerationResult {
            .patch_info = patch_info,
            .mesh       = std::move(mesh)
        });

        m_terrain_generation_sp.Signal();

        m_terrain_generation_flag.store(true);
    }, THREAD_POOL_BACKGROUND);

    m_enqueued_patches.Insert(patch->info.coord, task_ref);
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    const auto it = m_enqueued_patches.Find(patch->info.coord);

    if (it != m_enqueued_patches.End()) {
        DebugLog(
            LogType::Debug,
            "Unschedule task to generate terrain patch at coord [%f, %f]\n",
            it->first.x,
            it->first.y
        );

        TaskSystem::GetInstance().Unschedule(it->second);

        m_enqueued_patches.Erase(it);
    }

    if (patch->entity == nullptr) {
        DebugLog(
            LogType::Warn,
            "Terrain patch has no entity attached!\n"
        );

        return;
    }
    
    // @FIXME new ECS

    // for (const ID<Scene> &id : GetOwner()->GetScenes()) {
    //     if (auto scene = Handle<Scene>(id)) {
    //         DebugLog(LogType::Debug, "Remove terrain Entity with id #%u\n", patch->entity->GetID().value);

            // if (!scene->RemoveEntity(patch->entity)) {
            //     DebugLog(
            //         LogType::Warn,
            //         "Terrain entity with id #%u not in Scene! Could cause mem leak if cannot from entities from scene.\n",
            //         patch->entity->GetID().value
            //     );
            // }
    //     }
    // }

    patch->entity.Reset();
}

void TerrainPagingController::AddEnqueuedChunks()
{
    m_terrain_generation_sp.Wait();
    m_owned_terrain_mesh_queue = std::move(m_shared_terrain_mesh_queue);
    m_terrain_generation_sp.Signal();

    UInt num_chunks_added = 0u;

    while (m_owned_terrain_mesh_queue.Any()) {
        auto terrain_generation_result = m_owned_terrain_mesh_queue.Pop();
        const auto &patch_info = terrain_generation_result.patch_info;

        const auto enqueued_patches_it = m_enqueued_patches.Find(patch_info.coord);

        if (enqueued_patches_it == m_enqueued_patches.End()) {
            DebugLog(
                LogType::Info,
                "Terrain mesh at coord [%f, %f] no longer in map, must have been removed. Skipping.\n",
                patch_info.coord.x,
                patch_info.coord.y
            );

            continue;
        }

        m_enqueued_patches.Erase(enqueued_patches_it);

        DebugLog(
            LogType::Debug,
            "Add completed terrain mesh at coord [%f, %f]\n",
            patch_info.coord.x,
            patch_info.coord.y
        );

        auto mesh = std::move(terrain_generation_result.mesh);
        AssertThrow(mesh.IsValid());

        if (auto *patch = GetPatch(patch_info.coord)) {
            // @TODO new ECS

            AssertThrow(patch->entity.IsValid());
            // AssertThrow(!patch->entity->GetMesh().IsValid());

            ++num_chunks_added;

            // patch->entity->SetMesh(std::move(mesh));
        } else {
            DebugLog(
                LogType::Warn,
                "Patch at [%f, %f] does not exist after generation completeted!\n",
                patch_info.coord.x, patch_info.coord.y
            );
        }
    }

    DebugLog(LogType::Debug, "Added %u chunks\n", num_chunks_added);

    m_terrain_generation_flag.store(false);
}

void TerrainPagingController::Serialize(fbom::FBOMObject &out) const
{
    out.SetProperty("controller_name", fbom::FBOMString(), Memory::StrLen(controller_name), controller_name);

    out.SetProperty("seed", fbom::FBOMUnsignedInt(), m_seed);
    out.SetProperty("width", fbom::FBOMUnsignedInt(), m_patch_size.width);
    out.SetProperty("height", fbom::FBOMUnsignedInt(), m_patch_size.height);
    out.SetProperty("depth", fbom::FBOMUnsignedInt(), m_patch_size.depth);
    out.SetProperty("scale", fbom::FBOMVec3f(), m_scale);
    out.SetProperty("max_distance", fbom::FBOMFloat(), m_max_distance);
}

fbom::FBOMResult TerrainPagingController::Deserialize(const fbom::FBOMObject &in)
{
    in.GetProperty("seed").ReadUInt32(&m_seed);
    in.GetProperty("width").ReadUInt32(&m_patch_size.width);
    in.GetProperty("height").ReadUInt32(&m_patch_size.height);
    in.GetProperty("depth").ReadUInt32(&m_patch_size.depth);
    in.GetProperty("scale").ReadArrayElements(fbom::FBOMFloat(), 3, &m_scale);
    in.GetProperty("max_distance").ReadFloat(&m_max_distance);

    return fbom::FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2