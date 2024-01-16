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
static const Extent2D light_field_probe_dimensions = Extent2D { 256, 256 };
static const Extent2D framebuffer_dimensions = Extent2D { 256, 256 };
static const EnvProbeIndex invalid_probe_index = EnvProbeIndex();

static const UInt light_field_probe_lowres_packed_octahedron_size = 16;
static const UInt light_field_probe_radiance_packed_octahedron_size = 256;
static const UInt light_field_probe_irradiance_packed_octahedron_size = 128;
static const UInt light_field_probe_packed_border_size = 2;

static constexpr Bool light_field_use_texture_array = true;

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

static EnvProbeIndex GetProbeBindingIndex(const Vec3f &probe_position, const BoundingBox &grid_aabb, const Extent3D &grid_density)
{
    const Vec3f size_of_probe = (grid_aabb.GetExtent() / Vec3f(grid_density));
    
    Vec3f probe_diff_units = probe_position / size_of_probe + Vec3f(grid_density) * 0.5f;
    probe_diff_units = MathUtil::Trunc(probe_diff_units);

    const Int probe_index_at_point = (Int(probe_diff_units.x) * Int(grid_density.height) * Int(grid_density.depth))
        + (Int(probe_diff_units.y) * Int(grid_density.depth))
        + Int(probe_diff_units.z);

    EnvProbeIndex calculated_probe_index = invalid_probe_index;
    
    if (probe_index_at_point >= 0 && UInt(probe_index_at_point) < max_bound_ambient_probes) {
        calculated_probe_index = EnvProbeIndex(
            Extent3D { UInt(probe_diff_units.x), UInt(probe_diff_units.y), UInt(probe_diff_units.z) },
            grid_density
        );
    }

    return calculated_probe_index;
}

#pragma region Render commands

struct RENDER_COMMAND(UpdateEnvProbeAABBsInGrid) : renderer::RenderCommand
{
    EnvGrid     *grid;
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
        for (UInt update_index = 0; update_index < UInt(updates.Size()); update_index++) {
            grid->m_grid.SetProbeIndexOnRenderThread(update_index, updates[update_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSHData) : renderer::RenderCommand
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

struct RENDER_COMMAND(CreateEnvGridDescriptorSets) : renderer::RenderCommand
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

struct RENDER_COMMAND(CreateVoxelGridMipDescriptorSets) : renderer::RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateVoxelGridMipDescriptorSets)(FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets)
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

struct RENDER_COMMAND(CreateLightFieldStorageImages) : renderer::RenderCommand
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

struct RENDER_COMMAND(SetElementInGlobalDescriptorSet) : renderer::RenderCommand
{
    renderer::DescriptorType    type;
    renderer::DescriptorKey     key;
    UInt32                      element_index;
    ImageViewRef                value;

    RENDER_COMMAND(SetElementInGlobalDescriptorSet)(
        renderer::DescriptorType type,
        renderer::DescriptorKey key,
        UInt32 element_index,
        ImageViewRef value
    ) : type(type),
        key(key),
        element_index(element_index),
        value(std::move(value))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            switch (type) {
            case renderer::DescriptorType::IMAGE:
                descriptor_set_globals
                    ->GetOrAddDescriptor<renderer::ImageDescriptor>(key)
                    ->SetElementSRV(element_index, value);
                break;
            case renderer::DescriptorType::IMAGE_STORAGE:
                descriptor_set_globals
                    ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(key)
                    ->SetElementUAV(element_index, value);
                break;
            // case renderer::DescriptorType::UNIFORM_BUFFER:
            //     descriptor_set_globals
            //         ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(key)
            //         ->SetElementBuffer(element_index, value.Get<GPUBufferRef>());
            //     break;
            // case renderer::DescriptorType::STORAGE_BUFFER:
            //     descriptor_set_globals
            //         ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(key)
            //         ->SetElementBuffer(element_index, value.Get<GPUBufferRef>());
            //     break;
            // case renderer::DescriptorType::SAMPLER:
            //     descriptor_set_globals
            //         ->GetOrAddDescriptor<renderer::SamplerDescriptor>(key)
            //         ->SetElementSampler(element_index, value.Get<SamplerRef>());
            //     break;
            default:
                AssertThrowMsg(false, "Invalid descriptor type");
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(SetLightFieldBuffersInGlobalDescriptorSet) : renderer::RenderCommand
{
    ImageViewRef light_field_color_image_view;
    ImageViewRef light_field_normals_image_view;
    ImageViewRef light_field_depth_image_view;
    ImageViewRef light_field_depth_lowres_image_view;
    ImageViewRef light_field_irradiance_image_view;
    ImageViewRef light_field_filtered_distance_image_view;

    RENDER_COMMAND(SetLightFieldBuffersInGlobalDescriptorSet)(
        ImageViewRef light_field_color_image_view,
        ImageViewRef light_field_normals_image_view,
        ImageViewRef light_field_depth_image_view,
        ImageViewRef light_field_depth_lowres_image_view,
        ImageViewRef light_field_irradiance_image_view,
        ImageViewRef light_field_filtered_distance_image_view
    ) : light_field_color_image_view(std::move(light_field_color_image_view)),
        light_field_normals_image_view(std::move(light_field_normals_image_view)),
        light_field_depth_image_view(std::move(light_field_depth_image_view)),
        light_field_depth_lowres_image_view(std::move(light_field_depth_lowres_image_view)),
        light_field_irradiance_image_view(std::move(light_field_irradiance_image_view)),
        light_field_filtered_distance_image_view(std::move(light_field_filtered_distance_image_view))
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
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_DEPTH_BUFFER_LOWRES)
                ->SetElementSRV(0, light_field_depth_lowres_image_view);
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_IRRADIANCE_BUFFER)
                ->SetElementSRV(0, light_field_irradiance_image_view);
            
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::LIGHT_FIELD_FILTERED_DISTANCE_BUFFER)
                ->SetElementSRV(0, light_field_filtered_distance_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

EnvGrid::EnvGrid(EnvGridType type, const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(),
      m_type(type),
      m_aabb(aabb),
      m_voxel_grid_aabb(aabb),
      m_offset(aabb.GetCenter()),
      m_density(density),
      m_current_probe_index(0),
      m_flags(ENV_GRID_FLAGS_RESET_REQUESTED)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::SetCameraData(const Vec3f &position)
{
    Threads::AssertOnThread(THREAD_GAME | THREAD_TASK);

    const BoundingBox current_aabb = m_aabb;
    const Vec3f current_aabb_center = current_aabb.GetCenter();
    const Vec3f current_aabb_center_minus_offset = current_aabb_center - m_offset;

    const Vec3f aabb_extent = m_aabb.GetExtent();

    const Vec3f size_of_probe = SizeOfProbe();

    const Vec3i position_snapped {
        MathUtil::Floor<Float, Vec3i::Type>(position.x / size_of_probe.x),
        MathUtil::Floor<Float, Vec3i::Type>(position.y / size_of_probe.y),
        MathUtil::Floor<Float, Vec3i::Type>(position.z / size_of_probe.z)
    };

    const Vec3i current_grid_position {
        MathUtil::Floor<Float, Vec3i::Type>(current_aabb_center_minus_offset.x / size_of_probe.x),
        MathUtil::Floor<Float, Vec3i::Type>(current_aabb_center_minus_offset.y / size_of_probe.y),
        MathUtil::Floor<Float, Vec3i::Type>(current_aabb_center_minus_offset.z / size_of_probe.z)
    };

    if (current_grid_position == position_snapped) {
        return;
    }

    m_aabb.SetCenter(Vec3f(position_snapped) * size_of_probe + m_offset);

    // If the grid has moved, we need to offset the voxel grid.
    m_flags.BitOr(ENV_GRID_FLAGS_NEEDS_VOXEL_GRID_OFFSET, MemoryOrder::ACQUIRE_RELEASE);

    if (m_camera) {
        m_camera->SetTranslation(m_aabb.GetCenter());
    }

    Array<UInt> updates;
    updates.Resize(m_grid.num_probes);

    for (UInt x = 0; x < m_density.width; x++) {
        for (UInt y = 0; y < m_density.height; y++) {
            for (UInt z = 0; z < m_density.depth; z++) {
                const Vec3i coord { Vec3i::Type(x), Vec3i::Type(y), Vec3i::Type(z) };
                const Vec3i scrolled_coord = coord + position_snapped;

                const Vec3i scrolled_coord_clamped {
                    MathUtil::Mod(scrolled_coord.x, Vec3i::Type(m_density.width)),
                    MathUtil::Mod(scrolled_coord.y, Vec3i::Type(m_density.height)),
                    MathUtil::Mod(scrolled_coord.z, Vec3i::Type(m_density.depth))
                };
                
                const Int32 scrolled_clamped_index = scrolled_coord_clamped.x * m_density.width * m_density.height
                    + scrolled_coord_clamped.y * m_density.width
                    + scrolled_coord_clamped.z;

                const Int32 index = x * m_density.width * m_density.height
                    + y * m_density.width
                    + z;

                AssertThrow(scrolled_clamped_index >= 0);

                const BoundingBox new_probe_aabb {
                    m_aabb.min + (Vec3f(Float(x), Float(y), Float(z)) * size_of_probe),
                    m_aabb.min + (Vec3f(Float(x + 1), Float(y + 1), Float(z + 1)) * size_of_probe)
                };

                const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(scrolled_clamped_index);

                if (!probe.IsValid()) {
                    // Should not happen, but just in case
                    continue;
                }

                // If probe is at the edge of the grid, it will be moved to the opposite edge.
                // That means we need to re-render
                if (!probe->GetAABB().ContainsPoint(new_probe_aabb.GetCenter())) {
                    probe->SetAABB(new_probe_aabb);
                }

                updates[index] = scrolled_clamped_index;
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
    const Vec3f aabb_extent = m_aabb.GetExtent();

    const EnvProbeType probe_type = GetEnvProbeType();
    AssertThrow(probe_type != ENV_PROBE_TYPE_INVALID);

    const Extent2D probe_dimensions = GetProbeDimensions(probe_type);

    if (num_ambient_probes != 0) {
        // m_ambient_probes.Resize(num_ambient_probes);
        // m_env_probe_draw_proxies.Resize(num_ambient_probes);

        for (SizeType x = 0; x < m_density.width; x++) {
            for (SizeType y = 0; y < m_density.height; y++) {
                for (SizeType z = 0; z < m_density.depth; z++) {
                    const SizeType index = x * m_density.width * m_density.height
                        + y * m_density.width
                        + z;

                    const EnvProbeIndex binding_index = GetProbeBindingIndex(
                        m_aabb.min + (Vec3f(Float(x), Float(y), Float(z)) * SizeOfProbe()),
                        m_aabb,
                        m_density
                    );

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vec3f(Float(x), Float(y), Float(z)) * SizeOfProbe()),
                        m_aabb.min + (Vec3f(Float(x + 1), Float(y + 1), Float(z + 1)) * SizeOfProbe())
                    );

                    Handle<EnvProbe> probe = CreateObject<EnvProbe>(
                        scene,
                        env_probe_aabb,
                        probe_dimensions,
                        probe_type
                    );

                    m_grid.AddProbe(index, probe);
                    probe->m_grid_slot = index;

                    InitObject(probe);
                }
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    CreateVoxelGridData();

    if (GetEnvGridType() == ENV_GRID_TYPE_SH) {
        CreateSHData();
    } else if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        CreateLightFieldData();
    }

    {
        for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
            m_shader_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
        }

        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.aabb_max = Vector4(m_aabb.max, 1.0f);
        m_shader_data.aabb_min = Vector4(m_aabb.min, 1.0f);
        m_shader_data.voxel_grid_aabb_max = Vector4(m_voxel_grid_aabb.max, 1.0f);
        m_shader_data.voxel_grid_aabb_min = Vector4(m_voxel_grid_aabb.min, 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };
    }

    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -Int(probe_dimensions.width), Int(probe_dimensions.height),
            0.001f, (m_aabb.GetExtent() / Vec3f(m_density)).Max()
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
    m_framebuffer.Reset();

    if (GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD) {
        PUSH_RENDER_COMMAND(
            SetLightFieldBuffersInGlobalDescriptorSet,
            ImageViewRef::unset,
            ImageViewRef::unset,
            ImageViewRef::unset,
            ImageViewRef::unset,
            ImageViewRef::unset,
            ImageViewRef::unset
        );

    }

    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        renderer::DescriptorType::IMAGE,
        renderer::DescriptorKey::VOXEL_GRID_IMAGE,
        0,
        ImageViewRef::unset
    );

    SafeRelease(std::move(m_light_field_probe_descriptor_sets));

    SafeRelease(std::move(m_compute_sh_descriptor_sets));

    SafeRelease(std::move(m_voxelize_probe_descriptor_sets));
    SafeRelease(std::move(m_generate_voxel_grid_mipmaps_descriptor_sets));
    SafeRelease(std::move(m_voxel_grid_mips));
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_camera.IsValid());

    m_camera->Update(delta);

    GetParent()->GetScene()->CollectEntities(
        m_render_list,
        m_camera,
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .shader_definition = m_ambient_shader->GetCompiledShader().GetDefinition(),
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::NONE
                // .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
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

void EnvGrid::OnRender(Frame *frame)
{
    static constexpr UInt max_queued_probes_for_render = 1;

    Threads::AssertOnThread(THREAD_RENDER);
    const UInt num_ambient_probes = m_grid.num_probes;

    m_shader_data.enabled_indices_mask = { 0, 0, 0, 0 };

    const EnvGridFlags flags = m_flags.Get(MemoryOrder::ACQUIRE);

    EnvGridFlags new_flags = flags;

    const BoundingBox grid_aabb = m_aabb;

    if (flags & ENV_GRID_FLAGS_NEEDS_VOXEL_GRID_OFFSET) {
        DebugLog(LogType::Debug, "Offsetting voxel grid\n");

        // // Offset the voxel grid.
        // const Vec3f offset = (grid_aabb.GetCenter() - Vector4(m_shader_data.center).GetXYZ()) / SizeOfProbe();
        // const Vec3i offset_i {
        //     MathUtil::Floor<Float, Vec3i::Type>(offset.x),
        //     MathUtil::Floor<Float, Vec3i::Type>(offset.y),
        //     MathUtil::Floor<Float, Vec3i::Type>(offset.z)
        // };

        // OffsetVoxelGrid(frame, offset_i);

        new_flags &= ~ENV_GRID_FLAGS_NEEDS_VOXEL_GRID_OFFSET;
    }

    // if (flags & ENV_GRID_FLAGS_RESET_REQUESTED) {
    //     for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
    //         m_shader_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
    //     }

    //     new_flags &= ~ENV_GRID_FLAGS_RESET_REQUESTED;
    // }

    for (SizeType index = 0; index < std::size(m_shader_data.probe_indices); index++) {
        m_shader_data.probe_indices[index] = m_grid.GetEnvProbeOnRenderThread(index).GetID().ToIndex();

        // const Handle<EnvProbe> &probe = m_grid.GetEnvProbeOnRenderThread(index);

        // if (!probe.IsValid()) {
        //     continue;
        // }

        // const EnvProbeIndex binding_index = GetProbeBindingIndex(probe->GetDrawProxy().world_position, m_aabb, m_density);

        // if (binding_index == invalid_probe_index) {
        //     continue;
        // }

        // m_shader_data.probe_indices[binding_index.GetProbeIndex()] = probe.GetID().ToIndex();
    }

    if (g_engine->GetConfig().Get(CONFIG_DEBUG_ENV_GRID_PROBES)) {
        // Debug draw
        for (UInt index = 0; index < m_grid.num_probes; index++) {
            const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(index);

            if (!probe.IsValid()) {
                continue;
            }

            const ID<EnvProbe> probe_id = probe->GetID();

            Color probe_color { probe_id.Value() };

            if (m_next_render_indices.Contains(index)) {
                probe_color = Color(1.0f);
            }

            g_engine->GetImmediateMode().Sphere(
                probe->GetDrawProxy().world_position,
                0.5f,
                probe_color
            );
        }
    }

    // render enqueued probes
    while (m_next_render_indices.Any()) {
        RenderEnvProbe(frame, m_next_render_indices.Pop());
    }
    
    if (num_ambient_probes != 0) {
        // update probe positions in grid, choose next to render.
        AssertThrow(m_current_probe_index < m_grid.num_probes);

        const Vec3f &camera_position = g_engine->GetRenderState().GetCamera().camera.position;

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

                const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(indirect_index);
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

                        // m_shader_data.probe_indices[binding_index.GetProbeIndex()] = probe.GetID().ToIndex();

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
    m_shader_data.center = Vector4(grid_aabb.GetCenter(), 1.0f);
    m_shader_data.aabb_max = Vector4(grid_aabb.GetMax(), 1.0f);
    m_shader_data.aabb_min = Vector4(grid_aabb.GetMin(), 1.0f);
    m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

    g_engine->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);

    if (flags != new_flags) {
        m_flags.Set(new_flags, MemoryOrder::RELEASE);
    }
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateVoxelGridData()
{
    // Create our voxel grid texture
    m_voxel_grid_texture = CreateObject<Texture>(Texture3D(
        Extent3D { 256, 256, 256 },
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_voxel_grid_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_voxel_grid_texture);

    // Set our voxel grid texture in the global descriptor set so we can use it in shaders
    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        renderer::DescriptorType::IMAGE,
        renderer::DescriptorKey::VOXEL_GRID_IMAGE,
        0,
        m_voxel_grid_texture->GetImageView()
    );

    { // Voxel Grid mipmapping descriptor sets
        const UInt num_voxel_grid_mip_levels = m_voxel_grid_texture->GetImage()->NumMipmaps();
        m_voxel_grid_mips.Resize(num_voxel_grid_mip_levels);

        for (UInt mip_level = 0; mip_level < num_voxel_grid_mip_levels; mip_level++) {
            m_voxel_grid_mips[mip_level] = MakeRenderObject<renderer::ImageView>();

            // create descriptor sets for mip generation.
            DescriptorSetRef mip_descriptor_set = MakeRenderObject<renderer::DescriptorSet>();

            auto *mip_in = mip_descriptor_set
                ->AddDescriptor<renderer::ImageDescriptor>(0);

            if (mip_level == 0) {
                // first mip level -- input is the actual image
                mip_in->SetElementSRV(0, m_voxel_grid_texture->GetImageView());
            } else {
                mip_in->SetElementSRV(0, m_voxel_grid_mips[mip_level - 1]);
            }

            auto *mip_out = mip_descriptor_set
                ->AddDescriptor<renderer::StorageImageDescriptor>(1);

            mip_out->SetElementUAV(0, m_voxel_grid_mips[mip_level]);

            mip_descriptor_set
                ->AddDescriptor<renderer::SamplerDescriptor>(2)
                ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

            // DeferCreate(mip_descriptor_set, g_engine->GetGPUDevice(), &g_engine->GetGPUInstance()->GetDescriptorPool());

            m_generate_voxel_grid_mipmaps_descriptor_sets.PushBack(std::move(mip_descriptor_set));
        }
    }

    // Create shader, descriptor sets for voxelizing probes
    AssertThrowMsg(m_framebuffer.IsValid(), "Framebuffer must be created before voxelizing probes");
    AssertThrowMsg(m_framebuffer->GetAttachmentMap().Size() >= 3, "Framebuffer must have at least 3 attachments (color, normals, distances)");

    Handle<Shader> voxelize_probe_shader = g_shader_manager->GetOrCreate(HYP_NAME(EnvProbe_VoxelizeProbe), {{ "MODE_VOXELIZE" }});
    Handle<Shader> clear_voxels_shader = g_shader_manager->GetOrCreate(HYP_NAME(EnvProbe_VoxelizeProbe), {{ "MODE_CLEAR" }});
    Handle<Shader> offset_voxel_grid_shader = g_shader_manager->GetOrCreate(HYP_NAME(EnvProbe_VoxelizeProbe), {{ "MODE_OFFSET" }});

    const renderer::DescriptorTable descriptor_table = voxelize_probe_shader->GetCompiledShader().GetDefinition().GetDescriptorUsages().BuildDescriptorTable();

    const renderer::DescriptorSetDeclaration *descriptor_set_decl = descriptor_table.FindDescriptorSetDeclaration(HYP_NAME(VoxelizeProbeDescriptorSet));
    AssertThrow(descriptor_set_decl != nullptr);
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // create descriptor sets for depth pyramid generation.
        DescriptorSetRef descriptor_set = MakeRenderObject<renderer::DescriptorSet>(*descriptor_set_decl);

        if (auto *in_color_image_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InColorImage))) {
            in_color_image_descriptor->SetElementSRV(0, m_framebuffer->GetAttachmentUsages()[0]->GetImageView());
        } else {
            AssertThrowMsg(false, "Missing descriptor for InColorImage");
        }

        if (auto *in_normals_image_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InNormalsImage))) {
            in_normals_image_descriptor->SetElementSRV(0, m_framebuffer->GetAttachmentUsages()[1]->GetImageView());
        } else {
            AssertThrowMsg(false, "Missing descriptor for InNormalsImage");
        }

        if (auto *in_depth_image_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InDepthImage))) {
            in_depth_image_descriptor->SetElementSRV(0, m_framebuffer->GetAttachmentUsages()[2]->GetImageView());
        } else {
            AssertThrowMsg(false, "Missing descriptor for InDepthImage");
        }

        if (auto *sampler_linear_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(SamplerLinear))) {
            sampler_linear_descriptor->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());
        } else {
            AssertThrowMsg(false, "Missing descriptor for SamplerLinear");
        }

        if (auto *sampler_nearest_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(SamplerNearest))) {
            sampler_nearest_descriptor->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());
        } else {
            AssertThrowMsg(false, "Missing descriptor for SamplerNearest");
        }

        if (auto *env_grid_buffer_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(EnvGridBuffer))) {
            env_grid_buffer_descriptor->SetElementBuffer<EnvGridShaderData>(0, g_engine->GetRenderData()->env_grids.GetBuffer());
        } else {
            AssertThrowMsg(false, "Missing descriptor for EnvGridBuffer");
        }

        if (auto *env_probes_buffer_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(EnvProbesBuffer))) {
            env_probes_buffer_descriptor->SetElementBuffer(0, g_engine->GetRenderData()->env_probes.GetBuffer());
        } else {
            AssertThrowMsg(false, "Missing descriptor for EnvProbesBuffer");
        }

        if (auto *out_voxel_grid_image_descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(OutVoxelGridImage))) {
            out_voxel_grid_image_descriptor->SetElementUAV(0, m_voxel_grid_texture->GetImageView());
        } else {
            AssertThrowMsg(false, "Missing descriptor for OutVoxelGridImage");
        }

        DeferCreate(descriptor_set, g_engine->GetGPUDevice(), &g_engine->GetGPUInstance()->GetDescriptorPool());

        m_voxelize_probe_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    { // Compute shader to clear the voxel grid at a specific position
        m_clear_voxels = CreateObject<ComputePipeline>(
            clear_voxels_shader,
            Array<DescriptorSetRef> { m_voxelize_probe_descriptor_sets[0] }
        );

        InitObject(m_clear_voxels);
    }

    { // Compute shader to voxelize a probe into voxel grid
        m_voxelize_probe = CreateObject<ComputePipeline>(
            voxelize_probe_shader,
            Array<DescriptorSetRef> { m_voxelize_probe_descriptor_sets[0] }
        );

        InitObject(m_voxelize_probe);
    }

    { // Compute shader to 'offset' the voxel grid
        m_offset_voxel_grid = CreateObject<ComputePipeline>(
            offset_voxel_grid_shader,
            Array<DescriptorSetRef> { m_voxelize_probe_descriptor_sets[0] }
        );

        InitObject(m_offset_voxel_grid);
    }

    { // Compute shader to generate mipmaps for voxel grid
        m_generate_voxel_grid_mipmaps = CreateObject<ComputePipeline>(
            g_shader_manager->GetOrCreate(HYP_NAME(VCTGenerateMipmap)),
            Array<DescriptorSetRef> { m_generate_voxel_grid_mipmaps_descriptor_sets[0] } // only need to pass first to use for layout.
        );

        for (UInt mip_level = 0; mip_level < m_voxel_grid_texture->GetImage()->NumMipmaps(); mip_level++) {
            DeferCreate(
                m_voxel_grid_mips[mip_level],
                g_engine->GetGPUDevice(), 
                m_voxel_grid_texture->GetImage(),
                mip_level, 1,
                0, m_voxel_grid_texture->GetImage()->NumFaces()
            );

            DeferCreate(
                m_generate_voxel_grid_mipmaps_descriptor_sets[mip_level],
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            );
        }

        InitObject(m_generate_voxel_grid_mipmaps);
    }
}

void EnvGrid::CreateSHData()
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    m_sh_tiles_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    
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
            ->SetElementBuffer(m_sh_tiles_buffer);

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
            ->SetElementBuffer(g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);
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

void EnvGrid::CreateLightFieldData()
{
    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const Extent2D radiance_extent = light_field_use_texture_array
        ? Extent2D { light_field_probe_radiance_packed_octahedron_size, light_field_probe_radiance_packed_octahedron_size }
        : Extent2D {
              (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.width * m_density.height + light_field_probe_packed_border_size,
              (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size
          };

    const Extent2D irradiance_extent = light_field_use_texture_array
        ? Extent2D { light_field_probe_irradiance_packed_octahedron_size, light_field_probe_irradiance_packed_octahedron_size }
        : Extent2D {
              (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.width * m_density.height + light_field_probe_packed_border_size,
              (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size
          };

    const Extent2D lowres_extent = light_field_use_texture_array
        ? Extent2D { light_field_probe_lowres_packed_octahedron_size, light_field_probe_lowres_packed_octahedron_size }
        : Extent2D {
              (light_field_probe_lowres_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.width * m_density.height + light_field_probe_packed_border_size,
              (light_field_probe_lowres_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size
          };

    if constexpr (light_field_use_texture_array) {
        m_light_field_color_texture = CreateObject<Texture>(TextureArray(
            radiance_extent,
            InternalFormat::RGBA16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_color_texture = CreateObject<Texture>(Texture2D(
            radiance_extent,
            InternalFormat::RGBA16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_color_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_color_texture);

    if constexpr (light_field_use_texture_array) {
        m_light_field_normals_texture = CreateObject<Texture>(TextureArray(
            radiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_normals_texture = CreateObject<Texture>(Texture2D(
            radiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_normals_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_normals_texture);

    if constexpr (light_field_use_texture_array) {
        m_light_field_depth_texture = CreateObject<Texture>(TextureArray(
            radiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_depth_texture = CreateObject<Texture>(Texture2D(
            radiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_depth_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_depth_texture);

    if constexpr (light_field_use_texture_array) {
        m_light_field_lowres_depth_texture = CreateObject<Texture>(TextureArray(
            lowres_extent,
            InternalFormat::R32F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_lowres_depth_texture = CreateObject<Texture>(Texture2D(
            lowres_extent,
            InternalFormat::R32F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_lowres_depth_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_lowres_depth_texture);

    if constexpr (light_field_use_texture_array) {
        m_light_field_irradiance_texture = CreateObject<Texture>(TextureArray(
            irradiance_extent,
            InternalFormat::RGBA16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_irradiance_texture = CreateObject<Texture>(Texture2D(
            irradiance_extent,
            InternalFormat::RGBA16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_irradiance_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_irradiance_texture);

    if constexpr (light_field_use_texture_array) {
        m_light_field_filtered_distance_texture = CreateObject<Texture>(TextureArray(
            irradiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            m_density.Size()
        ));
    } else {
        m_light_field_filtered_distance_texture = CreateObject<Texture>(Texture2D(
            irradiance_extent,
            InternalFormat::RG16F,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));
    }

    m_light_field_filtered_distance_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_light_field_filtered_distance_texture);

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
            ->SetElementUAV(0, m_light_field_color_texture->GetImageView());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(6)
            ->SetElementUAV(0, m_light_field_normals_texture->GetImageView());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(7)
            ->SetElementUAV(0, m_light_field_depth_texture->GetImageView());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(8)
            ->SetElementUAV(0, m_light_field_lowres_depth_texture->GetImageView());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(9)
            ->SetElementUAV(0, m_light_field_irradiance_texture->GetImageView());

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(10)
            ->SetElementUAV(0, m_light_field_filtered_distance_texture->GetImageView());

        if (m_voxel_grid_texture.IsValid()) {
            m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(11)
                ->SetElementUAV(0, m_voxel_grid_texture->GetImageView());
        } else {
            m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(11)
                ->SetElementUAV(0, &g_engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage());
        }

        m_light_field_probe_descriptor_sets[frame_index]->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(12)
            ->SetElementBuffer<EnvGridShaderData>(0, g_engine->GetRenderData()->env_grids.GetBuffer());

        m_light_field_probe_descriptor_sets[frame_index]->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(13)
            ->SetElementBuffer(0, g_engine->GetRenderData()->env_probes.GetBuffer());
    }

    PUSH_RENDER_COMMAND(CreateEnvGridDescriptorSets, m_light_field_probe_descriptor_sets);

    { // Compute shader to pack rendered textures into the large grid
        m_pack_light_field_probe = CreateObject<ComputePipeline>(
            g_shader_manager->GetOrCreate(HYP_NAME(PackLightFieldProbe), {}),
            Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
        );

        InitObject(m_pack_light_field_probe);
    }

    { // Compute shader to copy border texels after rendering a probe into the grid
        m_copy_light_field_border_texels_irradiance = CreateObject<ComputePipeline>(
            g_shader_manager->GetOrCreate(HYP_NAME(LightField_CopyBorderTexels), {}),
            Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
        );

        InitObject(m_copy_light_field_border_texels_irradiance);
    }

    { // Compute shader to copy border texels after rendering a probe into the grid
        m_copy_light_field_border_texels_depth = CreateObject<ComputePipeline>(
            g_shader_manager->GetOrCreate(HYP_NAME(LightField_CopyBorderTexels), {{ "DEPTH" }}),
            Array<DescriptorSetRef> { m_light_field_probe_descriptor_sets[0] }
        );

        InitObject(m_copy_light_field_border_texels_depth);
    }

    // Update global desc sets
    PUSH_RENDER_COMMAND(
        SetLightFieldBuffersInGlobalDescriptorSet,
        m_light_field_color_texture->GetImageView(),
        m_light_field_normals_texture->GetImageView(),
        m_light_field_depth_texture->GetImageView(),
        m_light_field_lowres_depth_texture->GetImageView(),
        m_light_field_irradiance_texture->GetImageView(),
        m_light_field_filtered_distance_texture->GetImageView()
    );
}

void EnvGrid::CreateShader()
{
    ShaderProperties shader_properties(renderer::static_mesh_vertex_attributes, {
        "MODE_AMBIENT",
        "WRITE_NORMALS",
        "WRITE_MOMENTS"
    });

    m_ambient_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(RenderToCubemap),
        shader_properties
    );

    InitObject(m_ambient_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        framebuffer_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    m_framebuffer->AddAttachment(
        0,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            framebuffer_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // Normals
    m_framebuffer->AddAttachment(
        1,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            framebuffer_dimensions,
            InternalFormat::RG16F,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // Distance Moments
    m_framebuffer->AddAttachment(
        2,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            framebuffer_dimensions,
            InternalFormat::RG16F,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    m_framebuffer->AddAttachment(
        3,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            framebuffer_dimensions,
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

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    auto result = Result::OK;

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

    VoxelizeProbe(frame, probe_index);

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

    struct alignas(128)
    {
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
        0
    );

    m_clear_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_clear_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_compute_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_compute_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_finalize_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_finalize_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_finalize_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });
}

void EnvGrid::OffsetVoxelGrid(
    Frame *frame,
    Vec3i offset
)
{
    DebugLog(LogType::Debug, "OffsetVoxelGrid %d %d %d\n", offset.x, offset.y, offset.z);

    AssertThrow(m_voxel_grid_texture.IsValid());

    struct alignas(128)
    {
        ShaderVec4<UInt32>  probe_grid_position;
        ShaderVec4<UInt32>  cubemap_dimensions;
        ShaderVec4<Int32>   offset;
    } push_constants;

    Memory::MemSet(&push_constants, 0, sizeof(push_constants));

    push_constants.offset = { offset.x, offset.y, offset.z, 0 };

    m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_offset_voxel_grid->GetPipeline(),
        m_voxelize_probe_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
        }
    );

    m_offset_voxel_grid->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_offset_voxel_grid->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(), 
        Extent3D {
            (m_voxel_grid_texture->GetImage()->GetExtent().width + 7) / 8,
            (m_voxel_grid_texture->GetImage()->GetExtent().height + 7) / 8,
            (m_voxel_grid_texture->GetImage()->GetExtent().depth + 7) / 8
        }
    );

    m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

void EnvGrid::VoxelizeProbe(
    Frame *frame,
    UInt32 probe_index
)
{
    AssertThrow(m_voxel_grid_texture.IsValid());

    const Handle<EnvProbe> &probe = m_grid.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());
    AssertThrow(probe->m_grid_slot < max_bound_ambient_probes);
    AssertThrow(probe->m_grid_slot == probe_index);

    const ImageRef &color_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
    const Extent2D cubemap_dimensions = Extent2D(color_image->GetExtent());

    DescriptorSetRef &descriptor_set = m_voxelize_probe_descriptor_sets[frame->GetFrameIndex()];
    AssertThrow(descriptor_set.IsValid());

    struct alignas(128)
    {
        ShaderVec4<UInt32>  probe_grid_position;
        ShaderVec4<UInt32>  cubemap_dimensions;
        ShaderVec4<Float32> world_position;
    } push_constants;

    push_constants.probe_grid_position = {
        probe->m_grid_slot % m_density.width,
        (probe->m_grid_slot % (m_density.width * m_density.height)) / m_density.width,
        probe->m_grid_slot / (m_density.width * m_density.height),
        probe.GetID().ToIndex()
    };

    push_constants.cubemap_dimensions = { cubemap_dimensions.width, cubemap_dimensions.height, 0, 0 };

    push_constants.world_position = Vector4(probe->GetAABB().GetCenter(), 1.0f);
    
    {   // Clear our voxel grid at the start of each probe
        m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

        frame->GetCommandBuffer()->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_clear_voxels->GetPipeline(),
            m_voxelize_probe_descriptor_sets[frame->GetFrameIndex()],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
            }
        );

        m_clear_voxels->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
        m_clear_voxels->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(), 
            Extent3D {
                (cubemap_dimensions.width + 31) / 32,
                (cubemap_dimensions.height + 31) / 32,
                1
            }
        );
    }

    { // Voxelize probe
        m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

        frame->GetCommandBuffer()->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_voxelize_probe->GetPipeline(),
            m_voxelize_probe_descriptor_sets[frame->GetFrameIndex()],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
            }
        );

        m_voxelize_probe->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
        m_voxelize_probe->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(), 
            Extent3D {
                (cubemap_dimensions.width + 31) / 32,
                (cubemap_dimensions.height + 31) / 32,
                1
            }
        );
    }

    { // Generate mipmaps for the voxel grid

        m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

        const UInt num_mip_levels = m_voxel_grid_texture->GetImage()->NumMipmaps();
        const Extent3D voxel_image_extent = m_voxel_grid_texture->GetImage()->GetExtent();
        Extent3D mip_extent = voxel_image_extent;

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            const Extent3D prev_mip_extent = mip_extent;

            mip_extent.width = MathUtil::Max(1u, voxel_image_extent.width >> mip_level);
            mip_extent.height = MathUtil::Max(1u, voxel_image_extent.height >> mip_level);
            mip_extent.depth = MathUtil::Max(1u, voxel_image_extent.depth >> mip_level);
        
            if (mip_level != 0) {
                // put the mip into writeable state
                m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertSubResourceBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            frame->GetCommandBuffer()->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_generate_voxel_grid_mipmaps->GetPipeline(),
                m_generate_voxel_grid_mipmaps_descriptor_sets[mip_level],
                DescriptorSet::Index(0)
            );

            m_generate_voxel_grid_mipmaps->GetPipeline()->Bind(
                frame->GetCommandBuffer(),
                Pipeline::PushConstantData {
                    .voxel_mip_data = {
                        .mip_dimensions = renderer::ShaderVec4<UInt32>(Vec3u(mip_extent), 0),
                        .prev_mip_dimensions = renderer::ShaderVec4<UInt32>(Vec3u(prev_mip_extent), 0),
                        .mip_level = mip_level
                    }
                }
            );
            
            // dispatch to generate this mip level
            m_generate_voxel_grid_mipmaps->GetPipeline()->Dispatch(
                frame->GetCommandBuffer(),
                Extent3D {
                    (mip_extent.width + 7) / 8,
                    (mip_extent.height + 7) / 8,
                    (mip_extent.depth + 7) / 8
                }
            );

            // put this mip into readable state
            m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertSubResourceBarrier(
                frame->GetCommandBuffer(),
                renderer::ImageSubResource { .base_mip_level = mip_level },
                renderer::ResourceState::SHADER_RESOURCE
            );
        }

        // all mip levels have been transitioned into this state
        m_voxel_grid_texture->GetImage()->GetGPUImage()->SetResourceState(
            renderer::ResourceState::SHADER_RESOURCE
        );
    }
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
        ShaderVec4<UInt32> probe_offset_coord;
        ShaderVec4<UInt32> probe_offset_coord_lowres;
        ShaderVec4<UInt32> grid_dimensions;
    } push_constants;

    AssertThrow(probe->m_grid_slot < max_bound_ambient_probes);

    //temp
    AssertThrow(probe->m_grid_slot == probe_index);
    
    const Vec3u position_in_grid {
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
    
    push_constants.probe_offset_coord = {
        (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * (position_in_grid.y * m_density.width + position_in_grid.x),
        (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * position_in_grid.z,
        (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * (position_in_grid.y * m_density.width + position_in_grid.x),
        (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * position_in_grid.z
    };

    push_constants.probe_offset_coord_lowres = {
        (light_field_probe_lowres_packed_octahedron_size + light_field_probe_packed_border_size) * (position_in_grid.y * m_density.width + position_in_grid.x),
        (light_field_probe_lowres_packed_octahedron_size + light_field_probe_packed_border_size) * position_in_grid.z,
        0, 0
    };

    push_constants.grid_dimensions = {
        (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * (m_density.width * m_density.height) + light_field_probe_packed_border_size,
        (light_field_probe_radiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size,
        (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * (m_density.width * m_density.height) + light_field_probe_packed_border_size,
        (light_field_probe_irradiance_packed_octahedron_size + light_field_probe_packed_border_size) * m_density.depth + light_field_probe_packed_border_size
    };

    m_light_field_color_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_normals_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_depth_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_irradiance_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    m_light_field_lowres_depth_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    m_voxel_grid_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_pack_light_field_probe->GetPipeline(),
        m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, GetComponentIndex())
        }
    );

    m_pack_light_field_probe->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_pack_light_field_probe->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (light_field_probe_radiance_packed_octahedron_size + 31) / 32,
            (light_field_probe_radiance_packed_octahedron_size + 31) / 32,
            1
        }
    );

    // Temporarily commented out because I am increasing the size of the images to test

    // // Copy border texels for irradiance image

    // m_light_field_color_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    // frame->GetCommandBuffer()->BindDescriptorSet(
    //     g_engine->GetGPUInstance()->GetDescriptorPool(),
    //     m_copy_light_field_border_texels_irradiance->GetPipeline(),
    //     m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
    //     0
    // );

    // m_copy_light_field_border_texels_irradiance->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    // m_copy_light_field_border_texels_irradiance->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    // // Copy border texels for depth image

    // m_light_field_depth_texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    // frame->GetCommandBuffer()->BindDescriptorSet(
    //     g_engine->GetGPUInstance()->GetDescriptorPool(),
    //     m_copy_light_field_border_texels_depth->GetPipeline(),
    //     m_light_field_probe_descriptor_sets[frame->GetFrameIndex()],
    //     0
    // );

    // m_copy_light_field_border_texels_depth->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    // m_copy_light_field_border_texels_depth->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_light_field_color_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_normals_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_depth_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_irradiance_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_light_field_lowres_depth_texture->GetImage()->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2