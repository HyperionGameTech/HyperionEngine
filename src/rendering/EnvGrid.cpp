#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Result;

static const Extent2D num_tiles = { 4, 4 };
static const Extent2D sh_probe_dimensions = Extent2D { 16, 16 };
static const Extent2D light_field_probe_dimensions = Extent2D { 32, 32 };
static const EnvProbeIndex invalid_probe_index = EnvProbeIndex();

static const UInt light_field_probe_packed_octahedron_size = 16;
static const UInt light_field_probe_packed_border_size = 2;

static Extent2D GetProbeDimensions(EnvProbeType env_probe_type)
{
    switch (env_probe_type) {
    case ENV_PROBE_TYPE_AMBIENT:
        return sh_probe_dimensions;
    case ENV_PROBE_TYPE_LIGHT_FIELD:
        return light_field_probe_dimensions;
    default:
        AssertThrowMsg(false, "Invalid probe type");
    }

    return Extent2D { UInt(-1), UInt(-1) };
}

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
        HYPERION_BUBBLE_ERRORS(sh_tiles_buffer->Create(g_engine->GetGPUDevice(), sizeof(SHTile) * num_tiles.Size() * 6));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateEnvGridDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateEnvGridDescriptorSets)(FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets)
        : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (auto &descriptor_set : descriptor_sets) {
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(g_engine->GetGPUDevice(), &g_engine->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};


struct RENDER_COMMAND(CreateLightFieldStorageImages) : RenderCommand
{
    Array<LightFieldStorageImage> storage_images;

    RENDER_COMMAND(CreateLightFieldStorageImages)(Array<LightFieldStorageImage> storage_images)
        : storage_images(std::move(storage_images))
    {
    }

    virtual Result operator()()
    {
        for (LightFieldStorageImage &storage_image : storage_images) {
            AssertThrow(storage_image.image.IsValid());
            AssertThrow(storage_image.image_view.IsValid());

            HYPERION_BUBBLE_ERRORS(storage_image.image->Create(g_engine->GetGPUDevice()));
            HYPERION_BUBBLE_ERRORS(storage_image.image_view->Create(g_engine->GetGPUDevice(), storage_image.image));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(SetLightFieldBuffersInGlobalDescriptorSet) : RenderCommand
{
    ImageViewRef light_field_color_image_view;
    ImageViewRef light_field_normals_image_view;
    ImageViewRef light_field_depth_image_view;

    RENDER_COMMAND(SetLightFieldBuffersInGlobalDescriptorSet)(ImageViewRef light_field_color_image_view, ImageViewRef light_field_normals_image_view, ImageViewRef light_field_depth_image_view)
        : light_field_color_image_view(std::move(light_field_color_image_view)),
          light_field_normals_image_view(std::move(light_field_normals_image_view)),
          light_field_depth_image_view(std::move(light_field_depth_image_view))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_COLOR_BUFFER)
                ->SetElementSRV(0, light_field_color_image_view);
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_NORMALS_BUFFER)
                ->SetElementSRV(0, light_field_normals_image_view);
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_DEPTH_BUFFER)
                ->SetElementSRV(0, light_field_depth_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

EnvGrid::EnvGrid(EnvGridType type, const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(),
      m_type(type),
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
    
    const Vector3 position_offset = position;

    Vector3 position_snapped = Vector3(
        MathUtil::Floor<Float, Float>(position_offset.x / size_of_probe.x),
        MathUtil::Floor<Float, Float>(position_offset.y / size_of_probe.y),
        MathUtil::Floor<Float, Float>(position_offset.z / size_of_probe.z)
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

                probe->SetAABB(BoundingBox(
                    m_aabb.min + ((Vector3(Float(x), Float(y), Float(z)) + camera_position) * size_of_probe),
                    m_aabb.min + ((Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) + camera_position) * size_of_probe)
                ));
                // probe->SetNeedsRender(true);

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

    const EnvProbeType probe_type = GetEnvProbeType();
    AssertThrow(probe_type != ENV_PROBE_TYPE_INVALID);

    const Extent2D probe_dimensions = GetProbeDimensions(probe_type);

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
                        probe_dimensions,
                        probe_type
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

    if (GetEnvGridType() == ENV_GRID_TYPE_SH) {
        CreateSHData();
        CreateSHClipmapData();
    } else if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        CreateLightFieldData();
    }

    m_offset_center = m_aabb.GetCenter();

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
        const Vector3 size_of_probe = (m_aabb.GetExtent() / Vector3(m_density));

        m_camera = CreateObject<Camera>(
            90.0f,
            -Int(probe_dimensions.width), Int(probe_dimensions.height),
            0.001f, m_aabb.GetRadius()//size_of_probe.Max() * 0.5f//(m_aabb * (Vector3::one / Vector3(m_density))).GetRadius() + 2.0f
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
                HYPERION_PASS_ERRORS(attachment->Destroy(g_engine->GetGPUInstance()->GetDevice()), result);
            }

            env_grid.m_attachments.clear();

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyEnvGridFramebufferAttachments, *this);

    if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        PUSH_RENDER_COMMAND(
            SetLightFieldBuffersInGlobalDescriptorSet,
            ImageViewRef::unset,
            ImageViewRef::unset,
            ImageViewRef::unset
        );
    }
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
                .cull_faces = FaceCullMode::BACK,
                // .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
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
    const Vector3 size_of_probe = (grid_aabb.GetExtent() / Vector3(grid_density));
    const Vector3 probe_position_center = probe_position;

    Vector3 probe_diff_units = probe_position_center / size_of_probe + Vector3(grid_density) * 0.5f;
    probe_diff_units = MathUtil::Trunc(probe_diff_units);

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

    if (g_engine->GetConfig().Get(CONFIG_DEBUG_ENV_GRID_PROBES)) {
        // Debug draw
        for (UInt index = 0; index < m_grid.num_probes; index++) {
            const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(index);

            if (!probe.IsValid()) {
                continue;
            }

            const ID<EnvProbe> probe_id = probe->GetID();

            g_engine->GetImmediateMode().Sphere(
                probe->GetDrawProxy().world_position,
                0.35f,
                Color(probe_id.Value())
            );
        }
    }
    
    if (num_ambient_probes != 0) {
        // update probe positions in grid, choose next to render.
        AssertThrow(m_current_probe_index < m_grid.num_probes);

        const Vector3 &camera_position = g_engine->GetRenderState().GetCamera().camera.position;

        Array<Pair<UInt, Float>> indices_distances;
        indices_distances.Reserve(16);

        for (UInt i = 0; i < m_grid.num_probes; i++) {
            const UInt index = (m_current_probe_index + i) % m_grid.num_probes;
            const Handle<EnvProbe> &probe = m_grid.GetEnvProbeOnRenderThread(index);

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
                const UInt indirect_index = m_grid.GetEnvProbeIndexOnRenderThread(found_index);

                const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(indirect_index);//.GetEnvProbeOnRenderThread(found_index);
                AssertThrow(probe.IsValid());

                const EnvProbeIndex binding_index = GetProbeBindingIndex(probe->GetDrawProxy().world_position, grid_aabb, m_density);

                if (binding_index != invalid_probe_index) {
                    if (m_next_render_indices.Size() < max_queued_probes_for_render) {
                        EnvProbe::UpdateEnvProbeShaderData(
                            probe->GetID(),
                            probe->GetDrawProxy(),
                            ~0u,
                            probe->m_grid_slot,
                            m_density
                        );

                        m_shader_data.probe_indices[binding_index.GetProbeIndex()] = probe->GetID().ToIndex();
                        
                        // AssertThrow(indirect_index + 1 < max_bound_ambient_probes);
                        // m_shader_data.probe_indices[indirect_index + 1] = probe->GetID().ToIndex();

                        // render this probe next frame, since the data will have been updated on the gpu on start of the frame
                        m_next_render_indices.Push(indirect_index);

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

                // probe->SetNeedsRender(false);
            }
        }
    }
    
    m_shader_data.extent = Vector4(grid_aabb.GetExtent(), 1.0f);
    m_shader_data.center = Vector4(m_camera->GetDrawProxy().position, 1.0f);
    m_shader_data.aabb_max = Vector4(grid_aabb.max, 1.0f);
    m_shader_data.aabb_min = Vector4(grid_aabb.min, 1.0f);
    m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

    g_engine->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);

    if (GetEnvGridType() == EnvGridType::ENV_GRID_TYPE_SH) {
        ComputeClipmaps(frame);
    }

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
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    m_sh_tiles_buffer = MakeRenderObject<GPUBuffer>(StorageBuffer());
    
    PUSH_RENDER_COMMAND(
        CreateSHData,
        m_sh_tiles_buffer
    );

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_sh_descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>();

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetElementSRV(0, &g_engine->GetPlaceholderData().GetImageViewCube1x1R8());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_sh_tiles_buffer);

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
            ->SetElementBuffer(0, g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);
    }

    PUSH_RENDER_COMMAND(CreateEnvGridDescriptorSets, m_compute_sh_descriptor_sets);
    
    m_clear_sh = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_CLEAR" }}),
        Array<DescriptorSetRef> { m_compute_sh_descriptor_sets[0] }
    );

    InitObject(m_clear_sh);

    m_compute_sh = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_BUILD_COEFFICIENTS" }}),
        Array<DescriptorSetRef> { m_compute_sh_descriptor_sets[0] }
    );

    InitObject(m_compute_sh);

    m_finalize_sh = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(ComputeSH), {{ "MODE_FINALIZE" }}),
        Array<DescriptorSetRef> { m_compute_sh_descriptor_sets[0] }
    );

    InitObject(m_finalize_sh);
}

void EnvGrid::CreateSHClipmapData()
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_clipmaps_descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>();

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

        m_compute_clipmaps_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetElementUAV(0, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[0].image_view)
            ->SetElementUAV(1, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[1].image_view)
            ->SetElementUAV(2, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[2].image_view)
            ->SetElementUAV(3, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[3].image_view)
            ->SetElementUAV(4, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[4].image_view)
            ->SetElementUAV(5, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[5].image_view)
            ->SetElementUAV(6, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[6].image_view)
            ->SetElementUAV(7, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[7].image_view)
            ->SetElementUAV(8, g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps[8].image_view);

        // gbuffer textures
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // scene buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, g_engine->GetRenderData()->scenes.GetBuffer(frame_index).get());

        // camera buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(5)
            ->SetElementBuffer<CameraShaderData>(0, g_engine->GetRenderData()->cameras.GetBuffer(frame_index).get());

        // env grid buffer (dynamic)
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(6)
            ->SetElementBuffer<EnvGridShaderData>(0, g_engine->GetRenderData()->env_grids.GetBuffer(frame_index).get());

        // env probes buffer
        m_compute_clipmaps_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(7)
            ->SetElementBuffer(0, g_engine->GetRenderData()->env_probes.GetBuffer(frame_index).get());
    }

    PUSH_RENDER_COMMAND(CreateEnvGridDescriptorSets, m_compute_clipmaps_descriptor_sets);

    m_compute_clipmaps = CreateObject<ComputePipeline>(
        CreateObject<Shader>(g_engine->GetShaderCompiler().GetCompiledShader(HYP_NAME(ComputeSHClipmap))),
        Array<DescriptorSetRef> { m_compute_clipmaps_descriptor_sets[0] }
    );

    InitObject(m_compute_clipmaps);
}

void EnvGrid::ComputeClipmaps(Frame *frame)
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    const auto &scene_binding = g_engine->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();

    const auto &camera = g_engine->GetRenderState().GetCamera();
    
    const auto &clipmaps = g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps;

    AssertThrow(clipmaps[0].image.IsValid());
    const Extent3D &clipmap_extent = clipmaps[0].image->GetExtent();

    struct alignas(128) {
        ShaderVec4<UInt32> clipmap_dimensions;
        ShaderVec4<Float> cage_center_world;
    } push_constants;

    push_constants.clipmap_dimensions = { clipmap_extent[0], clipmap_extent[1], clipmap_extent[2], 0 };
    push_constants.cage_center_world = Vector4(camera.camera.position, 1.0f);

    for (auto &texture : g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    }

    g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_compute_clipmaps->GetPipeline(),
        m_compute_clipmaps_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
            HYP_RENDER_OBJECT_OFFSET(Camera, camera.id.ToIndex()),
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

    for (auto &texture : g_engine->GetRenderData()->spherical_harmonics_grid.clipmaps) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

void EnvGrid::CreateLightFieldData()
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);
        
    const Extent3D extent {
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.width * m_density.height + light_field_probe_packed_border_size,
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size,
        1
    };

    m_light_field_color_texture.image = MakeRenderObject<Image>(StorageImage(
        extent,
        InternalFormat::RGBA16F,
        ImageType::TEXTURE_TYPE_2D,
        nullptr
    ));

    m_light_field_color_texture.image_view = MakeRenderObject<ImageView>();

    m_light_field_normals_texture.image = MakeRenderObject<Image>(StorageImage(
        extent,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_2D,
        nullptr
    ));

    m_light_field_normals_texture.image_view = MakeRenderObject<ImageView>();

    m_light_field_depth_texture.image = MakeRenderObject<Image>(StorageImage(
        extent,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_2D,
        nullptr
    ));

    m_light_field_depth_texture.image_view = MakeRenderObject<ImageView>();

    PUSH_RENDER_COMMAND(
        CreateLightFieldStorageImages,
        Array<LightFieldStorageImage> {
            m_light_field_color_texture,
            m_light_field_normals_texture,
            m_light_field_depth_texture
        }
    );

    // Descriptor sets

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_light_field_probe_descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>();

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetElementSRV(0, &g_engine->GetPlaceholderData().GetImageViewCube1x1R8());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(1)
            ->SetElementSRV(0, &g_engine->GetPlaceholderData().GetImageViewCube1x1R8());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(2)
            ->SetElementSRV(0, &g_engine->GetPlaceholderData().GetImageViewCube1x1R8());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(3)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(4)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(5)
            ->SetElementUAV(0, m_light_field_color_texture.image_view);

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(6)
            ->SetElementUAV(0, m_light_field_normals_texture.image_view);

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(7)
            ->SetElementUAV(0, m_light_field_depth_texture.image_view);
    }

    PUSH_RENDER_COMMAND(CreateEnvGridDescriptorSets, m_light_field_probe_descriptor_sets);

    // Compute shader to pack rendered textures into the large grid

    m_pack_light_field_probe = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(PackLightFieldProbe), {}),
        Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
    );

    InitObject(m_pack_light_field_probe);

    // Compute shader to copy border texels after rendering a probe into the grid

    m_copy_light_field_border_texels_irradiance = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(LightField_CopyBorderTexels), {}),
        Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
    );

    InitObject(m_copy_light_field_border_texels_irradiance);

    m_copy_light_field_border_texels_depth = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(LightField_CopyBorderTexels), {{ "DEPTH" }}),
        Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
    );

    InitObject(m_copy_light_field_border_texels_depth);

    // Update global desc sets
    PUSH_RENDER_COMMAND(
        SetLightFieldBuffersInGlobalDescriptorSet,
        m_light_field_color_texture.image_view,
        m_light_field_normals_texture.image_view,
        m_light_field_depth_texture.image_view
    );
}

void EnvGrid::CreateShader()
{
    ShaderProperties shader_properties(renderer::static_mesh_vertex_attributes, { "MODE_AMBIENT" });

    if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        shader_properties.Set("WRITE_NORMALS");
        shader_properties.Set("WRITE_MOMENTS");
    }

    m_ambient_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(CubemapRenderer),
        shader_properties
    );

    InitObject(m_ambient_shader);
}

void EnvGrid::CreateFramebuffer()
{
    const Extent2D probe_dimensions = GetProbeDimensions(GetEnvProbeType());

    m_framebuffer = CreateObject<Framebuffer>(
        probe_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    m_framebuffer->AddAttachment(
        0,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            probe_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        // Normals
        m_framebuffer->AddAttachment(
            1,
            MakeRenderObject<Image>(renderer::FramebufferImageCube(
                probe_dimensions,
                InternalFormat::RG8,
                nullptr
            )),
            RenderPassStage::SHADER,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        // Moments
        m_framebuffer->AddAttachment(
            2,
            MakeRenderObject<Image>(renderer::FramebufferImageCube(
                probe_dimensions,
                InternalFormat::RG16F,
                nullptr
            )),
            RenderPassStage::SHADER,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );
    }

    m_framebuffer->AddAttachment(
        GetEnvGridType() == ENV_GRID_TYPE_SH
            ? 1
            : 3,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            probe_dimensions,
            g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
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
    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(probe_index);//GetEnvProbeOnRenderThread(probe_index);
    AssertThrow(probe.IsValid());

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    auto result = Result::OK;

    // DebugLog(LogType::Debug, "Render EnvProbe #%u\n", proxy->id.Value());

    {
        struct alignas(128) { UInt32 env_probe_index; } push_constants;
        push_constants.env_probe_index = probe->GetID().ToIndex();

        g_engine->GetRenderState().SetActiveEnvProbe(probe->GetID());
        g_engine->GetRenderState().BindScene(GetParent()->GetScene());

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

        g_engine->GetRenderState().UnbindScene();
        g_engine->GetRenderState().UnsetActiveEnvProbe();
    }
    
    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    ImageView *framebuffer_image_view = m_framebuffer->GetAttachmentUsages()[0]->GetImageView();
    
    switch (GetEnvGridType()) {
    case ENV_GRID_TYPE_SH:
        ComputeSH(frame, framebuffer_image, framebuffer_image_view, probe_index);

        break;

    case ENV_GRID_TYPE_LIGHT_FIELD:
        ComputeLightFieldData(frame, probe_index);

        break;
    }

    probe->SetNeedsRender(false);
}

void EnvGrid::ComputeSH(
    Frame *frame,
    const Image *image,
    const ImageView *image_view,
    UInt32 probe_index
)
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(probe_index);//GetEnvProbeOnRenderThread(probe_index);
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
    AssertThrow(probe->m_grid_slot == probe_index);//m_grid.GetEnvProbeIndexOnRenderThread(probe_index));
    
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
        ->ApplyUpdates(g_engine->GetGPUDevice());

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_clear_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_clear_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_clear_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_compute_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_compute_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_finalize_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * id.ToIndex())}
    );

    m_finalize_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_finalize_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });
}

void EnvGrid::ComputeLightFieldData(
    Frame *frame,
    UInt32 probe_index
)
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const ImageRef &color_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    const ImageViewRef &color_image_view = m_framebuffer->GetAttachmentUsages()[0]->GetImageView();
    
    const ImageRef &normals_image = m_framebuffer->GetAttachmentUsages()[1]->GetAttachment()->GetImage();
    const ImageViewRef &normals_image_view = m_framebuffer->GetAttachmentUsages()[1]->GetImageView();
    
    const ImageRef &depth_image = m_framebuffer->GetAttachmentUsages()[2]->GetAttachment()->GetImage();
    const ImageViewRef &depth_image_view = m_framebuffer->GetAttachmentUsages()[2]->GetImageView();

    m_light_field_probe_descriptor_sets[frame->GetFrameIndex()]
        ->GetDescriptor(0)
        ->SetElementSRV(0, color_image_view);

    m_light_field_probe_descriptor_sets[frame->GetFrameIndex()]
        ->GetDescriptor(1)
        ->SetElementSRV(0, normals_image_view);

    m_light_field_probe_descriptor_sets[frame->GetFrameIndex()]
        ->GetDescriptor(2)
        ->SetElementSRV(0, depth_image_view);

    m_light_field_probe_descriptor_sets[frame->GetFrameIndex()]
        ->ApplyUpdates(g_engine->GetGPUDevice());

    // Encode color, normals, depth as octahedral and store into respective sections of large texture map.

    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());

    const ID<EnvProbe> id = probe->GetID();

    struct alignas(128) {
        ShaderVec4<UInt32> probe_grid_position;
        ShaderVec4<UInt32> cubemap_dimensions;
        ShaderVec2<UInt32> probe_offset_coord;
        ShaderVec2<UInt32> grid_dimensions;
    } push_constants;

    AssertThrow(probe->m_grid_slot < max_bound_ambient_probes);

    //temp
    AssertThrow(probe->m_grid_slot == probe_index);
    
    const Vec3u position_in_grid = {
        probe->m_grid_slot % m_density.width,
        (probe->m_grid_slot % (m_density.width * m_density.height)) / m_density.width,
        probe->m_grid_slot / (m_density.width * m_density.height)
    };
    
    push_constants.probe_grid_position = {
        position_in_grid.x,
        position_in_grid.y,
        position_in_grid.z,
        probe->m_grid_slot
    };

    push_constants.cubemap_dimensions = { color_image->GetExtent().width, color_image->GetExtent().height, 0, 0 };
    
    push_constants.probe_offset_coord = Vec2u {
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * (position_in_grid.y * m_density.width + position_in_grid.x),
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * position_in_grid.z
    };

    push_constants.grid_dimensions = Vec2u {
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * (m_density.width * m_density.height) + light_field_probe_packed_border_size,
        (light_field_probe_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size
    };

    m_light_field_color_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_normals_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_depth_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_pack_light_field_probe->GetPipeline(),
        m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_pack_light_field_probe->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_pack_light_field_probe->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    // Copy border texels for irradiance image

    m_light_field_color_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_copy_light_field_border_texels_irradiance->GetPipeline(),
        m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_copy_light_field_border_texels_irradiance->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_copy_light_field_border_texels_irradiance->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    // Copy border texels for depth image

    m_light_field_depth_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_copy_light_field_border_texels_depth->GetPipeline(),
        m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_copy_light_field_border_texels_depth->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_copy_light_field_border_texels_depth->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_light_field_color_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_normals_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_depth_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2
