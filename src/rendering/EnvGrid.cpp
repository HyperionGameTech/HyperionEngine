#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

static const Extent2D num_tiles = { 4, 4 };
static const Extent2D ambient_probe_dimensions = Extent2D { 16, 16 };
static const EnvProbeIndex invalid_probe_index = EnvProbeIndex();

#pragma region Render commands

struct EnvProbeAABBUpdate
{
    UInt32 current_index;
    BoundingBox current_aabb;

    UInt32 new_index;
    BoundingBox new_aabb;
};

struct RENDER_COMMAND(UpdateEnvProbeAABBsInGrid) : RenderCommand
{
    EnvGrid *grid;
    Array<UInt> updates;

    RENDER_COMMAND(UpdateEnvProbeAABBsInGrid)(
        EnvGrid *grid,
        Array<UInt> &&updates
    ) : grid(grid),
        updates(std::move(updates))
    {
        AssertThrow(grid != nullptr);
        AssertSoftMsg(this->updates.Any(), "Pushed update command with zero updates, redundant\n");
    }

    virtual Result operator()() override
    {
        // grid->m_env_probe_draw_proxies = std::move(updates);

        for (UInt update_index = 0; update_index < UInt(updates.Size()); update_index++) {
            grid->m_grid.SetProbeIndexOnRenderThread(update_index, updates[update_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSHData) : RenderCommand
{
    GPUBufferRef sh_tiles_buffer;

    RENDER_COMMAND(CreateSHData)(
        GPUBufferRef sh_tiles_buffer
    ) : sh_tiles_buffer(std::move(sh_tiles_buffer))
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(sh_tiles_buffer->Create(Engine::Get()->GetGPUDevice(), sizeof(SHTile) * num_tiles.Size() * 6));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateComputeSHDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateComputeSHDescriptorSets)(FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets)
        : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (auto &descriptor_set : descriptor_sets) {
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(Engine::Get()->GetGPUDevice(), &Engine::Get()->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets)(FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets)
        : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (auto &descriptor_set : descriptor_sets) {
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(Engine::Get()->GetGPUDevice(), &Engine::Get()->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

EnvGrid::EnvGrid(const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(),
      m_aabb(aabb),
      m_density(density),
      m_current_probe_index(0),
      m_flags(ENV_GRID_FLAGS_RESET_REQUESTED)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::SetCameraData(const Vector3 &position)
{
    Threads::AssertOnThread(THREAD_GAME);

    const Vector3 aabb_extent = m_aabb.GetExtent();
    const Vector3 size_of_probe = (aabb_extent / Vector3(m_density));
    

    Vector3 position_snapped = Vector3(
        MathUtil::Floor<Float, Float>(position.x / size_of_probe.x),
        MathUtil::Floor<Float, Float>(position.y / size_of_probe.y),
        MathUtil::Floor<Float, Float>(position.z / size_of_probe.z)
    );

    Vector3 camera_position;

    if (m_camera) {
        camera_position = m_camera->GetTranslation() / size_of_probe;

        m_camera->SetTranslation(position_snapped * size_of_probe);
    }

    const Vector3 delta = camera_position - position_snapped;
    camera_position = position_snapped;

    const Vec3i movement_values = {
        MathUtil::Floor<Float, Vec3i::Type>(delta.x),
        MathUtil::Floor<Float, Vec3i::Type>(delta.y),
        MathUtil::Floor<Float, Vec3i::Type>(delta.z)
    };

    if (movement_values.x == 0 && movement_values.y == 0 && movement_values.z == 0) {
        return;
    }

    DebugLog(
        LogType::Debug,
        "Movement: (%d, %d, %d)\n",
        movement_values.x, movement_values.y, movement_values.z
    );

    // Put probes into a 3-dimensional array to scroll the probes

    Array<UInt> updates;
    updates.Resize(m_grid.num_probes);

    // Array<Array<Array<Handle<EnvProbe>>>> new_probes;
    Array<Array<Array<UInt>>> new_probes;

    new_probes.Resize(m_density.width);

    for (UInt x = 0; x < m_density.width; x++) {
        new_probes[x].Resize(m_density.height);

        for (UInt y = 0; y < m_density.height; y++) {
            new_probes[x][y].Resize(m_density.depth);

            for (UInt z = 0; z < m_density.depth; z++) {
                const Vec3i scrolled_indices = Vec3i { Int32(x), Int32(y), Int32(z) } - movement_values;

                const Vec3i density_i = Vec3i(m_density);
                const Vec3i clamped_indices = MathUtil::Mod(scrolled_indices, density_i);

                const Int32 index = clamped_indices.z * density_i.x * density_i.y
                    + clamped_indices.y * density_i.x
                    + clamped_indices.x;

                AssertThrow(index >= 0);

                const UInt probe_index = m_grid.GetEnvProbeIndexOnGameThread(index);

                const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(probe_index);
                AssertThrow(probe.IsValid());

                DebugLog(LogType::Debug, "Probe at index %d %d %d -> %d %d %d\n", x, y, z, clamped_indices.x, clamped_indices.y, clamped_indices.z);
                DebugLog(LogType::Debug, "  AABB: %f %f %f\n", probe->GetAABB().min.x, probe->GetAABB().min.y, probe->GetAABB().min.z);

                probe->SetAABB(BoundingBox(
                    m_aabb.min + ((Vector3(Float(x), Float(y), Float(z)) + camera_position) * size_of_probe),
                    m_aabb.min + ((Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) + camera_position) * size_of_probe)
                ));
                DebugLog(LogType::Debug, "  Now: %f %f %f\n", probe->GetAABB().min.x, probe->GetAABB().min.y, probe->GetAABB().min.z);

                new_probes[x][y][z] = probe_index;//probe;

                updates[z * m_density.width * m_density.height + y * m_density.width + x] = probe_index;
            }
        }
    }

    if (updates.Any()) {
        for (UInt update_index = 0; update_index < UInt(updates.Size()); update_index++) {
            AssertThrow(update_index < m_grid.num_probes);
            AssertThrow(updates[update_index] < m_grid.num_probes);

            m_grid.SetProbeIndexOnGameThread(update_index, updates[update_index]);
        }

        PUSH_RENDER_COMMAND(
            UpdateEnvProbeAABBsInGrid,
            this,
            std::move(updates)
        );
    }
}

void EnvGrid::Init()
{
    Handle<Scene> scene(GetParent()->GetScene()->GetID());
    AssertThrow(scene.IsValid());

    const SizeType num_ambient_probes = m_density.Size();
    const Vector3 aabb_extent = m_aabb.GetExtent();

    if (num_ambient_probes != 0) {
        // m_ambient_probes.Resize(num_ambient_probes);
        // m_env_probe_draw_proxies.Resize(num_ambient_probes);

        for (SizeType z = 0; z < m_density.depth; z++) {
            for (SizeType y = 0; y < m_density.height; y++) {
                for (SizeType x = 0; x < m_density.width; x++) {
                    const SizeType index = z * m_density.width * m_density.height
                         + y * m_density.width
                         + x;

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(m_density))),
                        m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(m_density)))
                    );

                    Handle<EnvProbe> probe = CreateObject<EnvProbe>(
                        scene,
                        env_probe_aabb,
                        ambient_probe_dimensions,
                        ENV_PROBE_TYPE_AMBIENT
                    );

                    const UInt probe_index = m_grid.AddProbe(probe);
                    probe->m_grid_slot = probe_index;

                    InitObject(probe);
                }
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    CreateSHData();
    CreateSHClipmapData();

    {
        for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
            m_shader_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
        }

        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.aabb_max = Vector4(m_aabb.max, 1.0f);
        m_shader_data.aabb_min = Vector4(m_aabb.min, 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };
    }

    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -Int(ambient_probe_dimensions.width), Int(ambient_probe_dimensions.height),
            0.15f, m_aabb.GetRadius()//(m_aabb * (Vector3::one / Vector3(m_density))).GetRadius() + 0.15f
        );

        m_camera->SetTranslation(m_aabb.GetCenter());

        m_camera->SetFramebuffer(m_framebuffer);
        InitObject(m_camera);

        m_render_list.SetCamera(m_camera);
    }

    DebugLog(LogType::Info, "Created %llu total ambient EnvProbes in grid\n", num_ambient_probes);
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGrid::OnRemoved()
{
    m_camera.Reset();
    m_render_list.Reset();
    m_ambient_shader.Reset();

    struct RENDER_COMMAND(DestroyEnvGridFramebufferAttachments) : RenderCommand
    {
        EnvGrid &env_grid;

        RENDER_COMMAND(DestroyEnvGridFramebufferAttachments)(EnvGrid &env_grid)
            : env_grid(env_grid)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            if (env_grid.m_framebuffer != nullptr) {
                for (auto &attachment : env_grid.m_attachments) {
                    env_grid.m_framebuffer->RemoveAttachmentUsage(attachment.get());
                }
            }

            for (auto &attachment : env_grid.m_attachments) {
                HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()), result);
            }

            env_grid.m_attachments.clear();

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyEnvGridFramebufferAttachments, *this);
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_camera.IsValid());

    m_camera->Update(delta);

    // @TODO: Only collect entities in next probe to render range.

    GetParent()->GetScene()->CollectEntities(
        m_render_list,
        m_camera,
        Bitset((1 << BUCKET_OPAQUE)),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::FRONT
            },
            m_ambient_shader->GetCompiledShader().GetDefinition(),
            Entity::InitInfo::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING // override flags -- require this flag to be set
        ),
        true // skip frustum culling
    );

    m_render_list.UpdateRenderGroups();

    for (UInt index = 0; index < m_grid.num_probes; index++) {
        // Don't worry about using the indirect index
        Handle<EnvProbe> &probe = m_grid.probes[index];
        AssertThrow(probe.IsValid());

        probe->Update(delta);
    }
}

static EnvProbeIndex GetProbeBindingIndex(const Vector3 &probe_position, const BoundingBox &grid_aabb, const Extent3D &grid_density)
{
    const Vector3 diff = probe_position - grid_aabb.GetCenter();
    const Vector3 probe_position_clamped = ((diff + (grid_aabb.GetExtent() * 0.5f)) / grid_aabb.GetExtent());

    Vector3 probe_diff_units = probe_position_clamped * Vector3(grid_density);
    probe_diff_units.x = std::fmodf(probe_diff_units.x, Float(grid_density.width));
    probe_diff_units.y = std::fmodf(probe_diff_units.y, Float(grid_density.height));
    probe_diff_units.z = std::fmodf(probe_diff_units.z, Float(grid_density.depth));
    probe_diff_units = MathUtil::Abs(MathUtil::Floor(probe_diff_units));

    const Int probe_index_at_point = (Int(probe_diff_units.x) * Int(grid_density.height) * Int(grid_density.depth))
        + (Int(probe_diff_units.y) * Int(grid_density.depth))
        + Int(probe_diff_units.z) + 1;

    EnvProbeIndex calculated_probe_index = invalid_probe_index;
    
    if (probe_index_at_point > 0 && UInt(probe_index_at_point) < max_bound_ambient_probes) {
        calculated_probe_index = EnvProbeIndex(
            Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) + 1 },
            grid_density
        );
    }

    return calculated_probe_index;
}

void EnvGrid::OnRender(Frame *frame)
{
    static constexpr UInt max_queued_probes_for_render = 1;

    Threads::AssertOnThread(THREAD_RENDER);
    const UInt num_ambient_probes = m_grid.num_probes;

    m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };

    const EnvGridFlags flags = m_flags.Get(MemoryOrder::ACQUIRE);
    EnvGridFlags new_flags = flags;

    const BoundingBox grid_aabb = m_aabb;

    if (flags & ENV_GRID_FLAGS_RESET_REQUESTED) {
        for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
            m_shader_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
        }

        new_flags &= ~ENV_GRID_FLAGS_RESET_REQUESTED;
    }

    // render enqueued probes
    while (m_next_render_indices.Any()) {
        RenderEnvProbe(frame, m_next_render_indices.Pop());
    }

    // Debug draw
    for (UInt index = 0; index < m_grid.num_probes; index++) {
        const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(index);

        if (!probe.IsValid()) {
            continue;
        }

        const ID<EnvProbe> probe_id = probe->GetID();

        Engine::Get()->GetImmediateMode().Sphere(
            probe->GetDrawProxy().world_position,
            0.15f,
            Color(probe_id.Value())
        );
    }
    
    if (num_ambient_probes != 0) {
        // update probe positions in grid, choose next to render.
        AssertThrow(m_current_probe_index < m_grid.num_probes);

        const Vector3 &camera_position = Engine::Get()->GetRenderState().GetCamera().camera.position;

        Array<Pair<UInt, Float>> indices_distances;
        indices_distances.Reserve(16);

        for (UInt i = 0; i < m_grid.num_probes; i++) {
            const UInt index = (m_current_probe_index + i) % m_grid.num_probes;
            const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(index);

            if (probe.IsValid() && probe->NeedsRender()) {
                indices_distances.PushBack({
                    index,
                    probe->GetDrawProxy().world_position.Distance(camera_position)
                });
            }
        }

        // std::sort(indices_distances.Begin(), indices_distances.End(), [](const auto &lhs, const auto &rhs) {
        //     return lhs.second < rhs.second;
        // });

        if (indices_distances.Any()) {
            for (const auto &it : indices_distances) {
                const UInt found_index = it.first;

                const Handle<EnvProbe> &probe = m_grid.GetEnvProbeOnRenderThread(found_index);
                AssertThrow(probe.IsValid());

                const EnvProbeIndex binding_index = GetProbeBindingIndex(probe->GetDrawProxy().world_position, grid_aabb, m_density);

                if (binding_index != invalid_probe_index) {
                    // DebugLog(LogType::Debug, "Render ambient probe at position %f, %f, %uf\n", data.position_in_grid.x, data.position_in_grid.y, data.position_in_grid.z);

                    if (m_next_render_indices.Size() < max_queued_probes_for_render) {
                        EnvProbe::UpdateEnvProbeShaderData(
                            probe->GetID(),
                            probe->GetDrawProxy(),
                            ~0u,
                            probe->m_grid_slot,
                            m_density
                        );

                        m_shader_data.probe_indices[binding_index.GetProbeIndex()] = probe->GetID().ToIndex();

                        // render this probe next frame, since the data will have been updated on the gpu on start of the frame
                        m_next_render_indices.Push(found_index);
                        m_current_probe_index = (found_index + 1) % m_grid.num_probes;
                    } else {
                        break;
                    }
                } else {
                    DebugLog(
                        LogType::Warn,
                        "Probe %u out of range of max bound env probes (position: %u, %u, %u)\n",
                        probe->GetID().Value(),
                        binding_index.position[0],
                        binding_index.position[1],
                        binding_index.position[2]
                    );
                }

                probe->SetNeedsRender(false);
            }
        }
    }
    
    m_shader_data.extent = Vector4(grid_aabb.GetExtent(), 1.0f);
    m_shader_data.center = Vector4(m_camera->GetDrawProxy().position, 1.0f);
    m_shader_data.aabb_max = Vector4(grid_aabb.max, 1.0f);
    m_shader_data.aabb_min = Vector4(grid_aabb.min, 1.0f);
    m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

    Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);

    ComputeClipmaps(frame);

    if (flags != new_flags) {
        m_flags.Set(new_flags, MemoryOrder::RELEASE);
    }
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateSHData()
{
    m_sh_tiles_buffer = RenderObjects::Make<GPUBuffer>(StorageBuffer());
    
    PUSH_RENDER_COMMAND(
        CreateSHData,
        m_sh_tiles_buffer
    );

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_sh_descriptor_sets[frame_index] = RenderObjects::Make<DescriptorSet>();

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageViewCube1x1R8());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_sh_tiles_buffer);

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
            ->SetElementBuffer(0, Engine::Get()->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);
    }

    PUSH_RENDER_COMMAND(CreateComputeSHDescriptorSets, m_compute_sh_descriptor_sets);
    
    m_clear_sh = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_CLEAR" }}),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_clear_sh);

    m_compute_sh = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_BUILD_COEFFICIENTS" }}),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_sh);

    m_finalize_sh = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_FINALIZE" }}),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_finalize_sh);
}

void EnvGrid::CreateSHClipmapData()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_clipmaps_descriptor_sets[frame_index] = RenderObjects::Make<DescriptorSet>();

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, Engine::Get()->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetElementUAV(0, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[0].image_view)
            ->SetElementUAV(1, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[1].image_view)
            ->SetElementUAV(2, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[2].image_view)
            ->SetElementUAV(3, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[3].image_view)
            ->SetElementUAV(4, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[4].image_view)
            ->SetElementUAV(5, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[5].image_view)
            ->SetElementUAV(6, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[6].image_view)
            ->SetElementUAV(7, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[7].image_view)
            ->SetElementUAV(8, Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps[8].image_view);

        // gbuffer textures
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // scene buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->GetRenderData()->scenes.GetBuffer(frame_index).get());

        // camera buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(5)
            ->SetElementBuffer<CameraShaderData>(0, Engine::Get()->GetRenderData()->cameras.GetBuffer(frame_index).get());

        // env grid buffer (dynamic)
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(6)
            ->SetElementBuffer<EnvGridShaderData>(0, Engine::Get()->GetRenderData()->env_grids.GetBuffer(frame_index).get());

        // env probes buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(7)
            ->SetElementBuffer(0, Engine::Get()->GetRenderData()->env_probes.GetBuffer(frame_index).get());
    }

    PUSH_RENDER_COMMAND(CreateComputeSHClipmapDescriptorSets, m_compute_clipmaps_descriptor_sets);

    m_compute_clipmaps = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(HYP_NAME(ComputeSHClipmap))),
        Array<const DescriptorSet *> { m_compute_clipmaps_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_clipmaps);
}

void EnvGrid::ComputeClipmaps(Frame *frame)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();

    const auto &camera = Engine::Get()->GetRenderState().GetCamera();

    DebugLog(LogType::Debug, "Camera #%u at %f, %f, %f\n", camera.id.Value(), camera.camera.position.x, camera.camera.position.y, camera.camera.position.z);
    
    const auto &clipmaps = Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps;

    AssertThrow(clipmaps[0].image.IsValid());
    const Extent3D &clipmap_extent = clipmaps[0].image->GetExtent();

    struct alignas(128) {
        ShaderVec4<UInt32> clipmap_dimensions;
        ShaderVec4<Float> cage_center_world;
    } push_constants;

    push_constants.clipmap_dimensions = { clipmap_extent[0], clipmap_extent[1], clipmap_extent[2], 0 };
    push_constants.cage_center_world = Vector4(Engine::Get()->GetRenderState().GetCamera().camera.position, 1.0f);

    for (auto &texture : Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    }

    Engine::Get()->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_compute_clipmaps->GetPipeline(),
        m_compute_clipmaps_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
            HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
        }
    );

    m_compute_clipmaps->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_clipmaps->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (clipmap_extent[0] + 7) / 8,
            (clipmap_extent[1] + 7) / 8,
            (clipmap_extent[2] + 7) / 8,
        }
    );

    for (auto &texture : Engine::Get()->GetRenderData()->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

void EnvGrid::CreateShader()
{
    m_ambient_shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(CubemapRenderer),
        ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_AMBIENT" })
    );

    InitObject(m_ambient_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        ambient_probe_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    m_framebuffer->AddAttachment(
        0,
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
            ambient_probe_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    m_framebuffer->AddAttachment(
        1,
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
            ambient_probe_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    InitObject(m_framebuffer);
}

void EnvGrid::RenderEnvProbe(
    Frame *frame,
    UInt32 probe_index
)
{
    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeOnRenderThread(probe_index);
    AssertThrow(probe.IsValid());

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    auto result = Result::OK;

    // DebugLog(LogType::Debug, "Render EnvProbe #%u\n", proxy->id.Value());

    {
        struct alignas(128) { UInt32 env_probe_index; } push_constants;
        push_constants.env_probe_index = probe->GetID().ToIndex();

        Engine::Get()->GetRenderState().SetActiveEnvProbe(probe->GetID());
        Engine::Get()->GetRenderState().BindScene(GetParent()->GetScene());

        m_render_list.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE)),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE)),
            nullptr,
            &push_constants
        );

        Engine::Get()->GetRenderState().UnbindScene();
        Engine::Get()->GetRenderState().UnsetActiveEnvProbe();
    }
    
    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    ImageView *framebuffer_image_view = m_framebuffer->GetAttachmentUsages()[0]->GetImageView();
    
    ComputeSH(frame, framebuffer_image, framebuffer_image_view, probe_index);

    probe->SetNeedsRender(false);
}

void EnvGrid::ComputeSH(
    Frame *frame,
    const Image *image,
    const ImageView *image_view,
    UInt32 probe_index
)
{
    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeOnRenderThread(probe_index);
    AssertThrow(probe.IsValid());

    const ID<EnvProbe> id = probe->GetID();

    AssertThrow(image != nullptr);
    AssertThrow(image_view != nullptr);

    struct alignas(128) {
        ShaderVec4<UInt32> probe_grid_position;
        ShaderVec4<UInt32> cubemap_dimensions;
    } push_constants;

    AssertThrow(probe->m_grid_slot < max_bound_ambient_probes);

    //temp
    AssertThrow(probe->m_grid_slot == m_grid.GetEnvProbeIndexOnRenderThread(probe_index));
    
    push_constants.probe_grid_position = {
        probe->m_grid_slot % m_density.width,
        (probe->m_grid_slot % (m_density.width * m_density.height)) / m_density.width,
        probe->m_grid_slot / (m_density.width * m_density.height),
        probe->m_grid_slot
    };
    push_constants.cubemap_dimensions = { image->GetExtent().width, image->GetExtent().height, 0, 0 };
    
    m_compute_sh_descriptor_sets[frame->GetFrameIndex()]
        ->GetDescriptor(0)
        ->SetElementSRV(0, image_view);

    m_compute_sh_descriptor_sets[frame->GetFrameIndex()]
        ->ApplyUpdates(Engine::Get()->GetGPUDevice());

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_clear_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_clear_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_clear_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_compute_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_compute_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    Engine::Get()->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_finalize_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_finalize_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_finalize_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });
}

} // namespace hyperion::v2
