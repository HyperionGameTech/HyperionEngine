/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;

#pragma region Globals

static const Vec2u sh_num_samples { 16, 16 };
static const Vec2u sh_num_tiles { 16, 16 };
static const Vec2u sh_probe_dimensions { 256, 256 };
static const uint32 sh_num_levels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(sh_num_samples.Max()) + 1));
static const bool sh_parallel_reduce = false;

static const uint32 max_queued_probes_for_render = 1;

static const InternalFormat ambient_probe_format = InternalFormat::R10G10B10A2;

static const Vec3u voxel_grid_dimensions { 256, 256, 256 };
static const InternalFormat voxel_grid_format = InternalFormat::RGBA8;

static const Vec2u framebuffer_dimensions { 256, 256 };
static const EnvProbeIndex invalid_probe_index = EnvProbeIndex();

const InternalFormat light_field_color_format = InternalFormat::RGBA8;
const InternalFormat light_field_depth_format = InternalFormat::RG16F;
static const uint32 irradiance_octahedron_size = 8;
static const Vec2u light_field_probe_dimensions { 32, 32 };

#pragma endregion Globals

#pragma region Helpers

static Vec2u GetProbeDimensions(EnvGridType env_grid_type)
{
    switch (env_grid_type) {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return sh_probe_dimensions;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return light_field_probe_dimensions;
    default:
        HYP_UNREACHABLE();
    }
}

static EnvProbeIndex GetProbeBindingIndex(const Vec3f &probe_position, const BoundingBox &grid_aabb, const Vec3u &grid_density)
{
    const Vec3f diff = probe_position - grid_aabb.GetCenter();
    const Vec3f pos_clamped = (diff / grid_aabb.GetExtent()) + Vec3f(0.5f);
    const Vec3f diff_units = MathUtil::Trunc(pos_clamped * Vec3f(grid_density));

    const int probe_index_at_point = (int(diff_units.x) * int(grid_density.y) * int(grid_density.z))
        + (int(diff_units.y) * int(grid_density.z))
        + int(diff_units.z);

    EnvProbeIndex calculated_probe_index = invalid_probe_index;

    if (probe_index_at_point >= 0 && uint32(probe_index_at_point) < max_bound_ambient_probes) {
        calculated_probe_index = EnvProbeIndex(
            Vec3u { uint32(diff_units.x), uint32(diff_units.y), uint32(diff_units.z) },
            grid_density
        );
    }

    return calculated_probe_index;
}

#pragma endregion Helpers

#pragma region Render commands

struct RENDER_COMMAND(SetElementInGlobalDescriptorSet) : renderer::RenderCommand
{
    Name                                        set_name;
    Name                                        element_name;
    renderer::DescriptorSetElement::ValueType   value;

    RENDER_COMMAND(SetElementInGlobalDescriptorSet)(
        Name set_name,
        Name element_name,
        renderer::DescriptorSetElement::ValueType value
    ) : set_name(set_name),
        element_name(element_name),
        value(std::move(value))
    {
    }

    virtual ~RENDER_COMMAND(SetElementInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (value.Is<GPUBufferRef>()) {
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(set_name, frame_index)
                    ->SetElement(element_name, value.Get<GPUBufferRef>());
            } else if (value.Is<ImageViewRef>()) {
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(set_name, frame_index)
                    ->SetElement(element_name, value.Get<ImageViewRef>());
            } else {
                AssertThrowMsg(false, "Not implemented");
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region Uniform buffer structs

struct LightFieldUniforms
{
    Vec4u   image_dimensions;
    Vec4u   probe_grid_position;
    Vec4u   dimension_per_probe;
    Vec4u   probe_offset_coord;

    uint32  num_bound_lights;
    uint32  _pad0;
    uint32  _pad1;
    uint32  _pad2;

    uint32  light_indices[16];
};

#pragma endregion Uniform buffer structs

#pragma region EnvProbeCollection

uint32 EnvProbeCollection::AddProbe(const Handle<EnvProbe> &env_probe)
{
    AssertThrow(env_probe.IsValid());
    AssertThrow(env_probe->IsReady());

    AssertThrow(num_probes < max_bound_ambient_probes);

    const uint32 index = num_probes++;

    env_probes[index] = env_probe;
    env_probe_render_resources[index] = TResourceHandle<EnvProbeRenderResource>(env_probe->GetRenderResource());
    indirect_indices[index] = index;
    indirect_indices[max_bound_ambient_probes + index] = index;

    return index;
}

// Must be called in EnvGrid::Init(), before probes are used from the render thread.
void EnvProbeCollection::AddProbe(uint32 index, const Handle<EnvProbe> &env_probe)
{
    AssertThrow(env_probe.IsValid());
    AssertThrow(env_probe->IsReady());

    AssertThrow(index < max_bound_ambient_probes);

    num_probes = MathUtil::Max(num_probes, index + 1);
        
    env_probes[index] = env_probe;
    env_probe_render_resources[index] = TResourceHandle<EnvProbeRenderResource>(env_probe->GetRenderResource());
    indirect_indices[index] = index;
    indirect_indices[max_bound_ambient_probes + index] = index;
}

#pragma region EnvProbeCollection

#pragma region EnvGrid

EnvGrid::EnvGrid(Name name, const Handle<Scene> &parent_scene, EnvGridOptions options)
    : RenderSubsystem(name),
      m_parent_scene(parent_scene),
      m_options(options),
      m_aabb(options.aabb),
      m_voxel_grid_aabb(options.aabb),
      m_offset(options.aabb.GetCenter()),
      m_current_probe_index(0)
{
    const Vec2u probe_dimensions = GetProbeDimensions(m_options.type);
    AssertThrow(probe_dimensions.Volume() != 0);

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(probe_dimensions.x), int(probe_dimensions.y),
        0.001f, m_aabb.GetRadius() * 2.0f
    );
    
    m_camera->SetName(Name::Unique("EnvGridCamera"));
    m_camera->SetTranslation(m_aabb.GetCenter());
}

EnvGrid::~EnvGrid()
{
    SafeRelease(std::move(m_ambient_shader));

    SafeRelease(std::move(m_framebuffer));

    SafeRelease(std::move(m_clear_sh));
    SafeRelease(std::move(m_compute_sh));
    SafeRelease(std::move(m_reduce_sh));
    SafeRelease(std::move(m_finalize_sh));

    SafeRelease(std::move(m_clear_voxels));
    SafeRelease(std::move(m_voxelize_probe));
    SafeRelease(std::move(m_offset_voxel_grid));
    SafeRelease(std::move(m_generate_voxel_grid_mipmaps));

    SafeRelease(std::move(m_compute_sh_descriptor_tables));
    SafeRelease(std::move(m_sh_tiles_buffers));

    SafeRelease(std::move(m_voxel_grid_mips));
    SafeRelease(std::move(m_generate_voxel_grid_mipmaps_descriptor_tables));

    SafeRelease(std::move(m_compute_irradiance));
    SafeRelease(std::move(m_compute_filtered_depth));
}

void EnvGrid::SetCameraData(const BoundingBox &aabb, const Vec3f &position)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    struct RENDER_COMMAND(UpdateEnvProbeAABBsInGrid) : renderer::RenderCommand
    {
        EnvGrid         *grid;
        Array<uint32>   updates;

        RENDER_COMMAND(UpdateEnvProbeAABBsInGrid)(EnvGrid *grid, Array<uint32> &&updates)
            : grid(grid),
              updates(std::move(updates))
        {
            AssertThrow(grid != nullptr);

            if (this->updates.Empty()) {
                HYP_LOG(EnvGrid, Warning, "Pushed update command with zero updates, redundant command invocation");
            }
        }

        virtual RendererResult operator()() override
        {
            for (uint32 update_index = 0; update_index < uint32(updates.Size()); update_index++) {
                grid->m_env_probe_collection.SetProbeIndexOnRenderThread(update_index, updates[update_index]);
            }

            HYPERION_RETURN_OK;
        }
    };

    m_aabb = aabb;

    const BoundingBox current_aabb = m_aabb;
    const Vec3f current_aabb_center = current_aabb.GetCenter();
    const Vec3f current_aabb_center_minus_offset = current_aabb_center - m_offset;

    const Vec3f size_of_probe = SizeOfProbe();

    const Vec3i position_snapped {
        MathUtil::Floor<float, Vec3i::Type>(position.x / size_of_probe.x),
        MathUtil::Floor<float, Vec3i::Type>(position.y / size_of_probe.y),
        MathUtil::Floor<float, Vec3i::Type>(position.z / size_of_probe.z)
    };

    const Vec3i current_grid_position {
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.x / size_of_probe.x),
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.y / size_of_probe.y),
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.z / size_of_probe.z)
    };

    if (current_grid_position == position_snapped) {
        return;
    }

    m_aabb.SetCenter(Vec3f(position_snapped) * size_of_probe + m_offset);

    if (m_camera) {
        m_camera->SetTranslation(m_aabb.GetCenter());
    }

    Array<uint32> updates;
    updates.Resize(m_env_probe_collection.num_probes);

    for (uint32 x = 0; x < m_options.density.x; x++) {
        for (uint32 y = 0; y < m_options.density.y; y++) {
            for (uint32 z = 0; z < m_options.density.z; z++) {
                const Vec3i coord { int(x), int(y), int(z) };
                const Vec3i scrolled_coord = coord + position_snapped;

                const Vec3i scrolled_coord_clamped {
                    MathUtil::Mod(scrolled_coord.x, int(m_options.density.x)),
                    MathUtil::Mod(scrolled_coord.y, int(m_options.density.y)),
                    MathUtil::Mod(scrolled_coord.z, int(m_options.density.z))
                };

                const int scrolled_clamped_index = scrolled_coord_clamped.x * m_options.density.x * m_options.density.y
                    + scrolled_coord_clamped.y * m_options.density.x
                    + scrolled_coord_clamped.z;

                const int index = x * m_options.density.x * m_options.density.y
                    + y * m_options.density.x
                    + z;

                AssertThrow(scrolled_clamped_index >= 0);

                const BoundingBox new_probe_aabb {
                    m_aabb.min + (Vec3f(float(x), float(y), float(z)) * size_of_probe),
                    m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * size_of_probe)
                };

                const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeDirect(scrolled_clamped_index);

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
        for (uint32 update_index = 0; update_index < uint32(updates.Size()); update_index++) {
            AssertThrow(update_index < m_env_probe_collection.num_probes);
            AssertThrow(updates[update_index] < m_env_probe_collection.num_probes);

            m_env_probe_collection.SetProbeIndexOnGameThread(update_index, updates[update_index]);
        }

        PUSH_RENDER_COMMAND(UpdateEnvProbeAABBsInGrid, this, std::move(updates));
    }
}

void EnvGrid::Init()
{
    HYP_SCOPE;

    const uint32 num_ambient_probes = m_options.density.Volume();
    const Vec3f aabb_extent = m_aabb.GetExtent();

    const Vec2u probe_dimensions = GetProbeDimensions(m_options.type);
    AssertThrow(probe_dimensions.Volume() != 0);

    if (num_ambient_probes != 0) {
        // m_ambient_probes.Resize(num_ambient_probes);
        // m_env_probe_draw_proxies.Resize(num_ambient_probes);

        for (uint32 x = 0; x < m_options.density.x; x++) {
            for (uint32 y = 0; y < m_options.density.y; y++) {
                for (uint32 z = 0; z < m_options.density.z; z++) {
                    const uint32 index = x * m_options.density.x * m_options.density.y
                        + y * m_options.density.x
                        + z;

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vec3f(float(x), float(y), float(z)) * SizeOfProbe()),
                        m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * SizeOfProbe())
                    );

                    Handle<EnvProbe> env_probe = CreateObject<EnvProbe>(
                        m_parent_scene,
                        env_probe_aabb,
                        probe_dimensions,
                        EnvProbeType::ENV_PROBE_TYPE_AMBIENT
                    );

                    env_probe->m_grid_slot = index;

                    InitObject(env_probe);

                    m_env_probe_collection.AddProbe(index, env_probe);
                }
            }
        }
    }

    CreateShader();
    CreateFramebuffer();
    CreateVoxelGridData();

    switch (GetEnvGridType()) {
    case ENV_GRID_TYPE_SH:
        CreateSphericalHarmonicsData();

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        CreateLightFieldData();

        break;
    default:
        break;
    }

    for (uint32 index = 0; index < ArraySize(m_shader_data.probe_indices); index++) {
        m_shader_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
    }

    m_shader_data.center = Vec4f(m_aabb.GetCenter(), 1.0f);
    m_shader_data.extent = Vec4f(m_aabb.GetExtent(), 1.0f);
    m_shader_data.aabb_max = Vec4f(m_aabb.max, 1.0f);
    m_shader_data.aabb_min = Vec4f(m_aabb.min, 1.0f);
    m_shader_data.density = { m_options.density.x, m_options.density.y, m_options.density.z, 0 };
    
    m_shader_data.voxel_grid_aabb_max = Vec4f(m_voxel_grid_aabb.max, 1.0f);
    m_shader_data.voxel_grid_aabb_min = Vec4f(m_voxel_grid_aabb.min, 1.0f);

    m_shader_data.light_field_image_dimensions = m_irradiance_texture.IsValid()
        ? Vec2i(m_irradiance_texture->GetExtent().GetXY())
        : Vec2i::Zero();

    m_shader_data.irradiance_octahedron_size = Vec2i(irradiance_octahedron_size, irradiance_octahedron_size);

    InitObject(m_camera);

    CameraRenderResource &camera_resource = m_camera->GetRenderResource();
    camera_resource.SetFramebuffer(m_framebuffer);
    
    m_camera_resource_handle = ResourceHandle(camera_resource);

    g_engine->GetRenderState()->BindEnvGrid(this);
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(g_game_thread);
}

void EnvGrid::OnRemoved()
{
    HYP_SCOPE;

    g_engine->GetRenderState()->UnbindEnvGrid(this);

    m_camera_resource_handle.Reset();
    m_camera.Reset();
    m_render_collector.Reset();
    m_ambient_shader.Reset();

    if (m_options.type == EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD) {
        PUSH_RENDER_COMMAND(
            SetElementInGlobalDescriptorSet,
            NAME("Scene"),
            NAME("LightFieldColorTexture"),
            g_engine->GetPlaceholderData()->GetImageView2D1x1R8()
        );

        PUSH_RENDER_COMMAND(
            SetElementInGlobalDescriptorSet,
            NAME("Scene"),
            NAME("LightFieldDepthTexture"),
            g_engine->GetPlaceholderData()->GetImageView2D1x1R8()
        );
    }

    if (m_options.flags & EnvGridFlags::USE_VOXEL_GRID) {
        PUSH_RENDER_COMMAND(
            SetElementInGlobalDescriptorSet,
            NAME("Scene"),
            NAME("VoxelGridTexture"),
            g_engine->GetPlaceholderData()->GetImageView3D1x1x1R8()
        );
    }

    SafeRelease(std::move(m_framebuffer));
    SafeRelease(std::move(m_clear_sh));
    SafeRelease(std::move(m_compute_sh));
    SafeRelease(std::move(m_reduce_sh));
    SafeRelease(std::move(m_finalize_sh));
    SafeRelease(std::move(m_compute_sh_descriptor_tables));

    SafeRelease(std::move(m_voxel_grid_mips));

    SafeRelease(std::move(m_generate_voxel_grid_mipmaps));
    SafeRelease(std::move(m_generate_voxel_grid_mipmaps_descriptor_tables));
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);
}

void EnvGrid::OnRender(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const CameraRenderResource &active_camera = g_engine->GetRenderState()->GetActiveCamera();

    const BoundingBox grid_aabb = m_aabb;

    if (!grid_aabb.IsValid() || !grid_aabb.IsFinite()) {
        return;
    }

    for (uint32 index = 0; index < ArraySize(m_shader_data.probe_indices); index++) {
        const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeOnRenderThread(index);

        m_shader_data.probe_indices[index] = probe->GetRenderResource().GetBufferIndex();
        AssertThrow(m_shader_data.probe_indices[index] != ~0u);
    }

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.env_grid_probes").ToBool()) {
        // Debug draw
        for (uint32 index = 0; index < m_env_probe_collection.num_probes; index++) {
            const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeDirect(index);

            if (!probe.IsValid()) {
                continue;
            }

            g_engine->GetDebugDrawer()->AmbientProbe(
                probe->GetRenderResource().GetBufferData().world_position.GetXYZ(),
                0.25f,
                probe
            );
        }
    }

    // render enqueued probes
    while (m_next_render_indices.Any()) {
        RenderEnvProbe(frame, m_next_render_indices.Pop());
    }

    if (m_env_probe_collection.num_probes != 0) {
        // update probe positions in grid, choose next to render.
        AssertThrow(m_current_probe_index < m_env_probe_collection.num_probes);

        const Vec3f &camera_position = active_camera.GetBufferData().camera_position.GetXYZ();

        Array<Pair<uint32, float>> indices_distances;
        indices_distances.Reserve(16);

        for (uint32 i = 0; i < m_env_probe_collection.num_probes; i++) {
            const uint32 index = (m_current_probe_index + i) % m_env_probe_collection.num_probes;
            const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeOnRenderThread(index);

            if (probe.IsValid() && probe->NeedsRender()) {
                indices_distances.PushBack({
                    index,
                    probe->GetRenderResource().GetBufferData().world_position.GetXYZ().Distance(camera_position)
                });
            }
        }

        // std::sort(indices_distances.Begin(), indices_distances.End(), [](const auto &lhs, const auto &rhs) {
        //     return lhs.second < rhs.second;
        // });

        if (indices_distances.Any()) {
            for (const auto &it : indices_distances) {
                const uint32 found_index = it.first;
                const uint32 indirect_index = m_env_probe_collection.GetEnvProbeIndexOnRenderThread(found_index);

                const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeDirect(indirect_index);
                AssertThrow(probe.IsValid());

                const Vec3f world_position = probe->GetRenderResource().GetBufferData().world_position.GetXYZ();

                const EnvProbeIndex binding_index = GetProbeBindingIndex(world_position, grid_aabb, m_options.density);

                if (binding_index != invalid_probe_index) {
                    if (m_next_render_indices.Size() < max_queued_probes_for_render) {
                        probe->GetRenderResource().UpdateRenderData(
                            ~0u,
                            indirect_index,
                            m_options.density
                        );

                        // render this probe in the next frame, since the data will have been updated on the gpu on start of the frame
                        m_next_render_indices.Push(indirect_index);

                        m_current_probe_index = (found_index + 1) % m_env_probe_collection.num_probes;
                    } else {
                        break;
                    }
                } else {
                    HYP_LOG(EnvGrid, Warning, "EnvProbe #{} out of range of max bound env probes (position: {}, world position: {}",
                        probe->GetID().Value(), binding_index.position, world_position);
                }

                probe->SetNeedsRender(false);
            }
        }
    }

    m_shader_data.extent = Vec4f(grid_aabb.GetExtent(), 1.0f);
    m_shader_data.center = Vec4f(grid_aabb.GetCenter(), 1.0f);
    m_shader_data.aabb_max = Vec4f(grid_aabb.GetMax(), 1.0f);
    m_shader_data.aabb_min = Vec4f(grid_aabb.GetMin(), 1.0f);
    m_shader_data.density = { m_options.density.x, m_options.density.y, m_options.density.z, 0 };

    g_engine->GetRenderData()->env_grids->Set(GetComponentIndex(), m_shader_data);
}

void EnvGrid::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateVoxelGridData()
{
    HYP_SCOPE;

    if (!(m_options.flags & EnvGridFlags::USE_VOXEL_GRID)) {
        return;
    }

    // Create our voxel grid texture
    m_voxel_grid_texture = CreateObject<Texture>(
        TextureDesc
        {
            ImageType::TEXTURE_TYPE_3D,
            voxel_grid_format,
            voxel_grid_dimensions,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            FilterMode::TEXTURE_FILTER_LINEAR,
        }
    );

    m_voxel_grid_texture->GetImage()->SetIsRWTexture(true);

    InitObject(m_voxel_grid_texture);

    // Set our voxel grid texture in the global descriptor set so we can use it in shaders
    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Scene"),
        NAME("VoxelGridTexture"),
        m_voxel_grid_texture->GetImageView()
    );

    // Create shader, descriptor sets for voxelizing probes
    AssertThrowMsg(m_framebuffer.IsValid(), "Framebuffer must be created before voxelizing probes");
    AssertThrowMsg(m_framebuffer->GetAttachmentMap().Size() >= 3, "Framebuffer must have at least 3 attachments (color, normals, distances)");

    ShaderRef voxelize_probe_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), {{ "MODE_VOXELIZE" }});
    ShaderRef offset_voxel_grid_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), {{ "MODE_OFFSET" }});
    ShaderRef clear_voxels_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_ClearProbeVoxels"));

    const renderer::DescriptorTableDeclaration descriptor_table_decl = voxelize_probe_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // create descriptor sets for depth pyramid generation.
        DescriptorSetRef descriptor_set = descriptor_table->GetDescriptorSet(NAME("VoxelizeProbeDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InColorImage"), m_framebuffer->GetAttachment(0)->GetImageView());
        descriptor_set->SetElement(NAME("InNormalsImage"), m_framebuffer->GetAttachment(1)->GetImageView());
        descriptor_set->SetElement(NAME("InDepthImage"), m_framebuffer->GetAttachment(2)->GetImageView());
        descriptor_set->SetElement(NAME("SamplerLinear"), g_engine->GetPlaceholderData()->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_engine->GetPlaceholderData()->GetSamplerNearest());
        descriptor_set->SetElement(NAME("EnvGridBuffer"), 0, sizeof(EnvGridShaderData), g_engine->GetRenderData()->env_grids->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("EnvProbesBuffer"), g_engine->GetRenderData()->env_probes->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("OutVoxelGridImage"), m_voxel_grid_texture->GetImageView());

        AssertThrow(m_voxel_grid_texture->GetImageView() != nullptr);
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    { // Compute shader to clear the voxel grid at a specific position
        m_clear_voxels = MakeRenderObject<ComputePipeline>(
            clear_voxels_shader,
            descriptor_table
        );

        DeferCreate(m_clear_voxels, g_engine->GetGPUDevice());
    }

    { // Compute shader to voxelize a probe into voxel grid
        m_voxelize_probe = MakeRenderObject<ComputePipeline>(
            voxelize_probe_shader,
            descriptor_table
        );

        DeferCreate(m_voxelize_probe, g_engine->GetGPUDevice());
    }

    { // Compute shader to 'offset' the voxel grid
        m_offset_voxel_grid = MakeRenderObject<ComputePipeline>(
            offset_voxel_grid_shader,
            descriptor_table
        );

        DeferCreate(m_offset_voxel_grid, g_engine->GetGPUDevice());
    }

    { // Compute shader to generate mipmaps for voxel grid
        ShaderRef generate_voxel_grid_mipmaps_shader = g_shader_manager->GetOrCreate(NAME("VCTGenerateMipmap"));
        AssertThrow(generate_voxel_grid_mipmaps_shader.IsValid());

        renderer::DescriptorTableDeclaration generate_voxel_grid_mipmaps_descriptor_table_decl = generate_voxel_grid_mipmaps_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        const uint32 num_voxel_grid_mip_levels = m_voxel_grid_texture->GetImage()->NumMipmaps();
        m_voxel_grid_mips.Resize(num_voxel_grid_mip_levels);

        for (uint32 mip_level = 0; mip_level < num_voxel_grid_mip_levels; mip_level++) {
            m_voxel_grid_mips[mip_level] = MakeRenderObject<ImageView>();

            DeferCreate(
                m_voxel_grid_mips[mip_level],
                g_engine->GetGPUDevice(),
                m_voxel_grid_texture->GetImage(),
                mip_level, 1,
                0, m_voxel_grid_texture->GetImage()->NumFaces()
            );

            // create descriptor sets for mip generation.
            DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(generate_voxel_grid_mipmaps_descriptor_table_decl);

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const DescriptorSetRef &mip_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapDescriptorSet"), frame_index);
                AssertThrow(mip_descriptor_set != nullptr);

                if (mip_level == 0) {
                    // first mip level -- input is the actual image
                    mip_descriptor_set->SetElement(NAME("InputTexture"), m_voxel_grid_texture->GetImageView());
                } else {
                    mip_descriptor_set->SetElement(NAME("InputTexture"), m_voxel_grid_mips[mip_level - 1]);
                }

                mip_descriptor_set->SetElement(NAME("OutputTexture"), m_voxel_grid_mips[mip_level]);
            }

            DeferCreate(descriptor_table, g_engine->GetGPUDevice());

            m_generate_voxel_grid_mipmaps_descriptor_tables.PushBack(std::move(descriptor_table));
        }

        m_generate_voxel_grid_mipmaps = MakeRenderObject<ComputePipeline>(
            generate_voxel_grid_mipmaps_shader,
            m_generate_voxel_grid_mipmaps_descriptor_tables[0]
        );

        DeferCreate(m_generate_voxel_grid_mipmaps, g_engine->GetGPUDevice());
    }
}

void EnvGrid::CreateSphericalHarmonicsData()
{
    HYP_SCOPE;

    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    m_sh_tiles_buffers.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++) {
        m_sh_tiles_buffers[i] = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

        DeferCreate(
            m_sh_tiles_buffers[i],
            g_engine->GetGPUDevice(), sizeof(SHTile) * (sh_num_tiles.x >> i) * (sh_num_tiles.y >> i)
        );
    }

    FixedArray<ShaderRef, 4> shaders = {
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), {{ "MODE_CLEAR" }}),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), {{ "MODE_BUILD_COEFFICIENTS" }}),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), {{ "MODE_REDUCE" }}),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), {{ "MODE_FINALIZE" }})
    };

    for (const ShaderRef &shader : shaders) {
        AssertThrow(shader.IsValid());
    }

    const renderer::DescriptorTableDeclaration descriptor_table_decl = shaders[0]->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    m_compute_sh_descriptor_tables.Resize(sh_num_levels);
    
    for (uint32 i = 0; i < sh_num_levels; i++) {
        m_compute_sh_descriptor_tables[i] = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &compute_sh_descriptor_set = m_compute_sh_descriptor_tables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame_index);
            AssertThrow(compute_sh_descriptor_set != nullptr);

            compute_sh_descriptor_set->SetElement(NAME("InColorCubemap"), g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InNormalsCubemap"), g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InDepthCubemap"), g_engine->GetPlaceholderData()->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InputSHTilesBuffer"), m_sh_tiles_buffers[i]);

            if (i != sh_num_levels - 1) {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), m_sh_tiles_buffers[i + 1]);
            } else {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), m_sh_tiles_buffers[i]);
            }
        }

        DeferCreate(m_compute_sh_descriptor_tables[i], g_engine->GetGPUDevice());
    }

    m_clear_sh = MakeRenderObject<ComputePipeline>(
        shaders[0],
        m_compute_sh_descriptor_tables[0]
    );

    DeferCreate(m_clear_sh, g_engine->GetGPUDevice());

    m_compute_sh = MakeRenderObject<ComputePipeline>(
        shaders[1],
        m_compute_sh_descriptor_tables[0]
    );

    DeferCreate(m_compute_sh, g_engine->GetGPUDevice());

    m_reduce_sh = MakeRenderObject<ComputePipeline>(
        shaders[2],
        m_compute_sh_descriptor_tables[0]
    );

    DeferCreate(m_reduce_sh, g_engine->GetGPUDevice());

    m_finalize_sh = MakeRenderObject<ComputePipeline>(
        shaders[3],
        m_compute_sh_descriptor_tables[0]
    );

    DeferCreate(m_finalize_sh, g_engine->GetGPUDevice());
}

void EnvGrid::CreateLightFieldData()
{
    HYP_SCOPE;

    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);
    
    m_irradiance_texture = CreateObject<Texture>(
        TextureDesc
        {
            ImageType::TEXTURE_TYPE_2D,
            light_field_color_format,
            Vec3u {
                (irradiance_octahedron_size + 2) * m_options.density.x * m_options.density.y + 2,
                (irradiance_octahedron_size + 2) * m_options.density.z + 2,
                1
            },
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR
        }
    );

    m_irradiance_texture->GetImage()->SetIsRWTexture(true);

    InitObject(m_irradiance_texture);

    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Scene"),
        NAME("LightFieldColorTexture"),
        m_irradiance_texture->GetImageView()
    );
    
    m_depth_texture = CreateObject<Texture>(
        TextureDesc
        {
            ImageType::TEXTURE_TYPE_2D,
            light_field_depth_format,
            Vec3u {
                (irradiance_octahedron_size + 2) * m_options.density.x * m_options.density.y + 2,
                (irradiance_octahedron_size + 2) * m_options.density.z + 2,
                1
            },
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR
        }
    );

    m_depth_texture->GetImage()->SetIsRWTexture(true);

    InitObject(m_depth_texture);

    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Scene"),
        NAME("LightFieldDepthTexture"),
        m_depth_texture->GetImageView()
    );

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        GPUBufferRef light_field_uniforms = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);

        DeferCreate(
            light_field_uniforms,
            g_engine->GetGPUDevice(),
            sizeof(LightFieldUniforms)
        );

        m_uniform_buffers.PushBack(std::move(light_field_uniforms));
    }

    ShaderRef compute_irradiance_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeIrradiance"));
    ShaderRef compute_filtered_depth_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeFilteredDepth"));
    ShaderRef copy_border_texels_shader = g_shader_manager->GetOrCreate(NAME("LightField_CopyBorderTexels"));

    Pair<ShaderRef, ComputePipelineRef &> shaders[] = {
        { compute_irradiance_shader, m_compute_irradiance },
        { compute_filtered_depth_shader, m_compute_filtered_depth },
        { copy_border_texels_shader, m_copy_border_texels }
    };

    for (Pair<ShaderRef, ComputePipelineRef &> &pair : shaders) {
        ShaderRef &shader = pair.first;
        ComputePipelineRef &pipeline = pair.second;

        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set = descriptor_table->GetDescriptorSet(NAME("LightFieldProbeDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffers[frame_index]);

            descriptor_set->SetElement(NAME("InColorImage"), m_framebuffer->GetAttachment(0)->GetImageView());
            descriptor_set->SetElement(NAME("InNormalsImage"), m_framebuffer->GetAttachment(1)->GetImageView());
            descriptor_set->SetElement(NAME("InDepthImage"), m_framebuffer->GetAttachment(2)->GetImageView());
            descriptor_set->SetElement(NAME("SamplerLinear"), g_engine->GetPlaceholderData()->GetSamplerLinear());
            descriptor_set->SetElement(NAME("SamplerNearest"), g_engine->GetPlaceholderData()->GetSamplerNearest());
            descriptor_set->SetElement(NAME("OutColorImage"), m_irradiance_texture->GetImageView());
            descriptor_set->SetElement(NAME("OutDepthImage"), m_depth_texture->GetImageView());
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        pipeline = MakeRenderObject<ComputePipeline>(shader, descriptor_table);
        DeferCreate(pipeline, g_engine->GetGPUDevice());
    }
}

void EnvGrid::CreateShader()
{
    HYP_SCOPE;

    ShaderProperties shader_properties(static_mesh_vertex_attributes, {
        "MODE_AMBIENT",
        "WRITE_NORMALS",
        "WRITE_MOMENTS"
    });

    m_ambient_shader = g_shader_manager->GetOrCreate(
        NAME("RenderToCubemap"),
        shader_properties
    );

    AssertThrow(m_ambient_shader.IsValid());
}

void EnvGrid::CreateFramebuffer()
{
    HYP_SCOPE;

    m_framebuffer = MakeRenderObject<Framebuffer>(
        framebuffer_dimensions,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    m_framebuffer->AddAttachment(
        0,
        ambient_probe_format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // Normals
    m_framebuffer->AddAttachment(
        1,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // Distance Moments
    AttachmentRef moments_attachment = m_framebuffer->AddAttachment(
        2,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // Set clear color for moments to be infinity
    moments_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

    m_framebuffer->AddAttachment(
        3,
        g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
}

void EnvGrid::RenderEnvProbe(Frame *frame, uint32 probe_index)
{
    HYP_SCOPE;

    const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    CameraRenderResource &camera_render_resource = static_cast<CameraRenderResource &>(*m_camera_resource_handle);

    // Bind a directional light
    TResourceHandle<LightRenderResource> *light_render_resource_handle = nullptr;

    {
        auto &directional_lights = g_engine->GetRenderState()->bound_lights[uint32(LightType::DIRECTIONAL)];

        if (directional_lights.Any()) {
            light_render_resource_handle = &directional_lights[0];

            g_engine->GetRenderState()->SetActiveLight(**light_render_resource_handle);
        }
    }

    g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(probe->GetRenderResource()));
    g_engine->GetRenderState()->SetActiveScene(m_parent_scene.Get());

    m_render_collector.CollectDrawCalls(
        frame,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    m_render_collector.ExecuteDrawCalls(
        frame,
        camera_render_resource,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    g_engine->GetRenderState()->UnsetActiveScene();
    g_engine->GetRenderState()->UnsetActiveEnvProbe();

    if (light_render_resource_handle != nullptr) {
        g_engine->GetRenderState()->UnsetActiveLight();
    }

    // Bind sky if it exists
    bool is_sky_set = false;

    if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
        g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front()));

        is_sky_set = true;
    }

    switch (GetEnvGridType()) {
    case ENV_GRID_TYPE_SH:
        ComputeEnvProbeIrradiance_SphericalHarmonics(frame, probe);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        ComputeEnvProbeIrradiance_LightField(frame, probe);

        break;
    default:
        break;
    }

    if (is_sky_set) {
        g_engine->GetRenderState()->UnsetActiveEnvProbe();
    }

    if (m_options.flags & EnvGridFlags::USE_VOXEL_GRID) {
        VoxelizeProbe(frame, probe_index);
    }

    probe->SetNeedsRender(false);
}

void EnvGrid::ComputeEnvProbeIrradiance_SphericalHarmonics(Frame *frame, const Handle<EnvProbe> &probe)
{
    HYP_SCOPE;

    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_SH);

    const uint32 probe_index = probe->m_grid_slot;

    const AttachmentRef &color_attachment = m_framebuffer->GetAttachment(0);
    const AttachmentRef &normals_attachment = m_framebuffer->GetAttachment(1);
    const AttachmentRef &depth_attachment = m_framebuffer->GetAttachment(2);

    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();
    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource = g_engine->GetRenderState()->GetActiveEnvProbe();

    // Bind a directional light
    TResourceHandle<LightRenderResource> *light_render_resource_handle = nullptr;

    {
        auto &directional_lights = g_engine->GetRenderState()->bound_lights[uint32(LightType::DIRECTIONAL)];

        if (directional_lights.Any()) {
            light_render_resource_handle = &directional_lights[0];
        }
    }

    const Vec2u cubemap_dimensions = color_attachment->GetImage()->GetExtent().GetXY();

    struct alignas(128)
    {
        Vec4u   probe_grid_position;
        Vec4u   cubemap_dimensions;
        Vec4u   level_dimensions;
        Vec4f   world_position;
    } push_constants;

    push_constants.probe_grid_position = {
        probe_index % m_options.density.x,
        (probe_index % (m_options.density.x * m_options.density.y)) / m_options.density.x,
        probe_index / (m_options.density.x * m_options.density.y),
        probe_index
    };

    push_constants.cubemap_dimensions = Vec4u { cubemap_dimensions, 0, 0 };

    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    for (const DescriptorTableRef &descriptor_set_ref : m_compute_sh_descriptor_tables) {
        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InColorCubemap"), color_attachment->GetImageView());

        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InNormalsCubemap"), normals_attachment->GetImageView());
        
        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InDepthCubemap"), depth_attachment->GetImageView());

        descriptor_set_ref->Update(g_engine->GetGPUDevice(), frame->GetFrameIndex());
    }

    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
        frame,
        m_sh_tiles_buffers[0],
        renderer::ResourceState::UNORDERED_ACCESS
    );

    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(frame,
        g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer,
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_clear_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    g_engine->GetGPUDevice()->GetAsyncCompute()->Dispatch(
        frame,
        m_clear_sh,
        Vec3u { 1, 1, 1 },
        m_compute_sh_descriptor_tables[0],
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_render_resource_handle != nullptr ? (*light_render_resource_handle)->GetBufferIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
        frame,
        m_sh_tiles_buffers[0],
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_compute_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    g_engine->GetGPUDevice()->GetAsyncCompute()->Dispatch(
        frame,
        m_compute_sh,
        Vec3u { 1, 1, 1 },
        m_compute_sh_descriptor_tables[0],
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_render_resource_handle != nullptr ? (*light_render_resource_handle)->GetBufferIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    // Parallel reduce
    if (sh_parallel_reduce) {
        for (uint32 i = 1; i < sh_num_levels; i++) {
            g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
                frame,
                m_sh_tiles_buffers[i - 1],
                renderer::ResourceState::UNORDERED_ACCESS
            );
            
            const Vec2u prev_dimensions {
                MathUtil::Max(1u, sh_num_samples.x >> (i - 1)),
                MathUtil::Max(1u, sh_num_samples.y >> (i - 1))
            };

            const Vec2u next_dimensions {
                MathUtil::Max(1u, sh_num_samples.x >> i),
                MathUtil::Max(1u, sh_num_samples.y >> i)
            };

            AssertThrow(prev_dimensions.x >= 2);
            AssertThrow(prev_dimensions.x > next_dimensions.x);
            AssertThrow(prev_dimensions.y > next_dimensions.y);

            push_constants.level_dimensions = {
                prev_dimensions.x,
                prev_dimensions.y,
                next_dimensions.x,
                next_dimensions.y
            };

            m_reduce_sh->SetPushConstants(&push_constants, sizeof(push_constants));

            g_engine->GetGPUDevice()->GetAsyncCompute()->Dispatch(
                frame,
                m_reduce_sh,
                Vec3u { 1, (next_dimensions.x + 3) / 4, (next_dimensions.y + 3) / 4 },
                m_compute_sh_descriptor_tables[i - 1],
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                            { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_render_resource_handle != nullptr ? (*light_render_resource_handle)->GetBufferIndex() : 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                        }
                    }
                }
            );
        }
    }

    const uint32 finalize_sh_buffer_index = sh_parallel_reduce ? sh_num_levels - 1 : 0;

    // Finalize - build into final buffer
    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
        frame,
        m_sh_tiles_buffers[finalize_sh_buffer_index],
        renderer::ResourceState::UNORDERED_ACCESS
    );

    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
        frame,
        g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer,
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_finalize_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    g_engine->GetGPUDevice()->GetAsyncCompute()->Dispatch(
        frame,
        m_finalize_sh,
        Vec3u { 1, 1, 1 },
        m_compute_sh_descriptor_tables[finalize_sh_buffer_index],
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_render_resource_handle != nullptr ? (*light_render_resource_handle)->GetBufferIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    g_engine->GetGPUDevice()->GetAsyncCompute()->InsertBarrier(
        frame,
        g_engine->GetRenderData()->spherical_harmonics_grid.sh_grid_buffer,
        renderer::ResourceState::UNORDERED_ACCESS
    );
}

void EnvGrid::ComputeEnvProbeIrradiance_LightField(Frame *frame, const Handle<EnvProbe> &probe)
{
    HYP_SCOPE;

    AssertThrow(GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const uint32 probe_index = probe->m_grid_slot;

    { // Update uniform buffer
        LightFieldUniforms uniforms;
        Memory::MemSet(&uniforms, 0, sizeof(uniforms));

        uniforms.image_dimensions = Vec4u { m_irradiance_texture->GetExtent(), 0 };

        uniforms.probe_grid_position = {
            probe_index % m_options.density.x,
            (probe_index % (m_options.density.x * m_options.density.y)) / m_options.density.x,
            probe_index / (m_options.density.x * m_options.density.y),
            probe->GetRenderResource().GetBufferIndex()
        };

        uniforms.dimension_per_probe = { irradiance_octahedron_size, irradiance_octahedron_size, 0, 0 };

        uniforms.probe_offset_coord = {
            (irradiance_octahedron_size + 2) * (uniforms.probe_grid_position.x * m_options.density.y + uniforms.probe_grid_position.y) + 1,
            (irradiance_octahedron_size + 2) * uniforms.probe_grid_position.z + 1,
            0, 0
        };

        const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.light_indices));
        uint32 num_bound_lights = 0;

        for (uint32 light_type = 0; light_type < uint32(LightType::POINT/*LightType::MAX*/); light_type++) {
            if (num_bound_lights >= max_bound_lights) {
                break;
            }

            for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
                if (num_bound_lights >= max_bound_lights) {
                    break;
                }

                uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
            }
        }

        uniforms.num_bound_lights = num_bound_lights;

        m_uniform_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);
    }

    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();
    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();

    // Used for sky
    const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource = g_engine->GetRenderState()->GetActiveEnvProbe();

    m_irradiance_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    m_compute_irradiance->Bind(frame->GetCommandBuffer());
    
    m_compute_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_compute_irradiance,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    m_compute_irradiance->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            (irradiance_octahedron_size + 7) / 8,
            (irradiance_octahedron_size + 7) / 8,
            1
        }
    );

    m_compute_filtered_depth->Bind(frame->GetCommandBuffer());
    
    m_compute_filtered_depth->GetDescriptorTable()->Bind(
        frame,
        m_compute_filtered_depth,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    m_compute_filtered_depth->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            (irradiance_octahedron_size + 7) / 8,
            (irradiance_octahedron_size + 7) / 8,
            1
        }
    );

    m_depth_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    m_copy_border_texels->Bind(frame->GetCommandBuffer());
    
    m_copy_border_texels->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    m_copy_border_texels->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u { ((irradiance_octahedron_size * 4) + 255) / 256, 1, 1 }
    );

    m_irradiance_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    m_depth_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

void EnvGrid::OffsetVoxelGrid(
    Frame *frame,
    Vec3i offset
)
{
    HYP_SCOPE;

    AssertThrow(m_voxel_grid_texture.IsValid());

    struct alignas(128)
    {
        Vec4u   probe_grid_position;
        Vec4u   cubemap_dimensions;
        Vec4i   offset;
    } push_constants;

    Memory::MemSet(&push_constants, 0, sizeof(push_constants));

    push_constants.offset = { offset.x, offset.y, offset.z, 0 };

    m_voxel_grid_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    m_offset_voxel_grid->SetPushConstants(&push_constants, sizeof(push_constants));

    m_offset_voxel_grid->Bind(frame->GetCommandBuffer());
    
    m_offset_voxel_grid->GetDescriptorTable()->Bind(
        frame,
        m_offset_voxel_grid,
        {
            {
                NAME("VoxelizeProbeDescriptorSet"),
                {
                    { NAME("EnvGridBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) }
                }
            }
        }
    );

    m_offset_voxel_grid->Dispatch(
        frame->GetCommandBuffer(),
        (m_voxel_grid_texture->GetImage()->GetExtent() + Vec3u(7)) / Vec3u(8)
    );

    m_voxel_grid_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

void EnvGrid::VoxelizeProbe(
    Frame *frame,
    uint32 probe_index
)
{
    AssertThrow(m_voxel_grid_texture.IsValid());

    const Vec3u &voxel_grid_texture_extent = m_voxel_grid_texture->GetImage()->GetExtent();

    // size of a probe in the voxel grid
    const Vec3u probe_voxel_extent = voxel_grid_texture_extent / m_options.density;

    const Handle<EnvProbe> &probe = m_env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());
    AssertThrow(probe->IsReady());

    const ImageRef &color_image = m_framebuffer->GetAttachment(0)->GetImage();
    const Vec3u cubemap_dimensions = color_image->GetExtent();

    struct alignas(128)
    {
        Vec4u   probe_grid_position;
        Vec4u   voxel_texture_dimensions;
        Vec4u   cubemap_dimensions;
        Vec4f   world_position;
    } push_constants;

    push_constants.probe_grid_position = {
        probe_index % m_options.density.x,
        (probe_index % (m_options.density.x * m_options.density.y)) / m_options.density.x,
        probe_index / (m_options.density.x * m_options.density.y),
        probe->GetRenderResource().GetBufferIndex()
    };

    push_constants.voxel_texture_dimensions = Vec4u(voxel_grid_texture_extent, 0);
    push_constants.cubemap_dimensions = Vec4u(cubemap_dimensions, 0);
    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    color_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

    if (false) {   // Clear our voxel grid at the start of each probe
        m_voxel_grid_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

        m_clear_voxels->SetPushConstants(&push_constants, sizeof(push_constants));

        m_clear_voxels->Bind(frame->GetCommandBuffer());

        m_clear_voxels->GetDescriptorTable()->Bind(
            frame,
            m_clear_voxels,
            {
                {
                    NAME("VoxelizeProbeDescriptorSet"),
                    {
                        { NAME("EnvGridBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) }
                    }
                }
            }
        );

        m_clear_voxels->Dispatch(
            frame->GetCommandBuffer(),
            (probe_voxel_extent + Vec3u(7)) / Vec3u(8)
        );
    }

    { // Voxelize probe
        m_voxel_grid_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

        m_voxelize_probe->SetPushConstants(&push_constants, sizeof(push_constants));
        m_voxelize_probe->Bind(frame->GetCommandBuffer());

        m_voxelize_probe->GetDescriptorTable()->Bind(
            frame,
            m_voxelize_probe,
            {
                {
                    NAME("VoxelizeProbeDescriptorSet"),
                    {
                        { NAME("EnvGridBuffer"), ShaderDataOffset<EnvGridShaderData>(GetComponentIndex()) }
                    }
                }
            }
        );

        m_voxelize_probe->Dispatch(
            frame->GetCommandBuffer(),
            Vec3u {
                (cubemap_dimensions.x + 31) / 32,
                (cubemap_dimensions.y + 31) / 32,
                1
            }
        );
    }

    { // Generate mipmaps for the voxel grid

        m_voxel_grid_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

        const uint32 num_mip_levels = m_voxel_grid_texture->GetImage()->NumMipmaps();

        const Vec3u voxel_image_extent = m_voxel_grid_texture->GetImage()->GetExtent();
        Vec3u mip_extent = voxel_image_extent;

        struct alignas(128)
        {
            Vec4u   mip_dimensions;
            Vec4u   prev_mip_dimensions;
            uint32  mip_level;
        } push_constant_data;

        for (uint32 mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            const Vec3u prev_mip_extent = mip_extent;

            mip_extent.x = MathUtil::Max(1u, voxel_image_extent.x >> mip_level);
            mip_extent.y = MathUtil::Max(1u, voxel_image_extent.y >> mip_level);
            mip_extent.z = MathUtil::Max(1u, voxel_image_extent.z >> mip_level);

            push_constant_data.mip_dimensions = Vec4u { mip_extent.x, mip_extent.y, mip_extent.z, 0 };
            push_constant_data.prev_mip_dimensions = Vec4u { prev_mip_extent.x, prev_mip_extent.y, prev_mip_extent.z, 0 };
            push_constant_data.mip_level = mip_level;

            if (mip_level != 0) {
                // put the mip into writeable state
                m_voxel_grid_texture->GetImage()->InsertSubResourceBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            m_generate_voxel_grid_mipmaps_descriptor_tables[mip_level]->Bind(
                frame,
                m_generate_voxel_grid_mipmaps,
                { }
            );

            m_generate_voxel_grid_mipmaps->SetPushConstants(&push_constant_data, sizeof(push_constant_data));

            m_generate_voxel_grid_mipmaps->Bind(frame->GetCommandBuffer());

            // dispatch to generate this mip level
            m_generate_voxel_grid_mipmaps->Dispatch(
                frame->GetCommandBuffer(),
                (mip_extent + Vec3u(7)) / Vec3u(8)
            );

            // put this mip into readable state
            m_voxel_grid_texture->GetImage()->InsertSubResourceBarrier(
                frame->GetCommandBuffer(),
                renderer::ImageSubResource { .base_mip_level = mip_level },
                renderer::ResourceState::SHADER_RESOURCE
            );
        }

        // all mip levels have been transitioned into this state
        m_voxel_grid_texture->GetImage()->SetResourceState(
            renderer::ResourceState::SHADER_RESOURCE
        );
    }
}

#pragma endregion EnvGrid

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Scene, EnvGridsBuffer, 1, sizeof(EnvGridShaderData), true);

} // namespace renderer

} // namespace hyperion
