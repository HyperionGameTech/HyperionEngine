/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/AsyncCompute.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region EnvProbeGridIndex

struct EnvProbeGridIndex
{
    Vec3u position;
    Vec3u grid_size;

    // defaults such that GetProbeIndex() == ~0u
    // because (~0u * 0 * 0) + (~0u * 0) + ~0u == ~0u
    EnvProbeGridIndex()
        : position { ~0u, ~0u, ~0u },
          grid_size { 0, 0, 0 }
    {
    }

    EnvProbeGridIndex(const Vec3u& position, const Vec3u& grid_size)
        : position(position),
          grid_size(grid_size)
    {
    }

    EnvProbeGridIndex(const EnvProbeGridIndex& other) = default;
    EnvProbeGridIndex& operator=(const EnvProbeGridIndex& other) = default;
    EnvProbeGridIndex(EnvProbeGridIndex&& other) noexcept = default;
    EnvProbeGridIndex& operator=(EnvProbeGridIndex&& other) noexcept = default;
    ~EnvProbeGridIndex() = default;

    HYP_FORCE_INLINE uint32 GetProbeIndex() const
    {
        return (position.x * grid_size.y * grid_size.z)
            + (position.y * grid_size.z)
            + position.z;
    }

    HYP_FORCE_INLINE bool operator<(uint32 value) const
    {
        return GetProbeIndex() < value;
    }

    HYP_FORCE_INLINE bool operator==(uint32 value) const
    {
        return GetProbeIndex() == value;
    }

    HYP_FORCE_INLINE bool operator!=(uint32 value) const
    {
        return GetProbeIndex() != value;
    }

    HYP_FORCE_INLINE bool operator<(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() < other.GetProbeIndex();
    }

    HYP_FORCE_INLINE bool operator==(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() == other.GetProbeIndex();
    }

    HYP_FORCE_INLINE bool operator!=(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() != other.GetProbeIndex();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetProbeIndex());

        return hc;
    }
};

#pragma endregion EnvProbeGridIndex

#pragma region Global Constants

static const Vec2u sh_num_samples { 16, 16 };
static const Vec2u sh_num_tiles { 16, 16 };
static const uint32 sh_num_levels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(sh_num_samples.Max()) + 1));
static const bool sh_parallel_reduce = false;

static const uint32 max_queued_probes_for_render = 1;

static const EnvProbeGridIndex invalid_probe_index = EnvProbeGridIndex();

#pragma endregion Globals Constants

#pragma region Helpers

static EnvProbeGridIndex GetProbeBindingIndex(const Vec3f& probe_position, const BoundingBox& grid_aabb, const Vec3u& grid_density)
{
    const Vec3f diff = probe_position - grid_aabb.GetCenter();
    const Vec3f pos_clamped = (diff / grid_aabb.GetExtent()) + Vec3f(0.5f);
    const Vec3f diff_units = MathUtil::Trunc(pos_clamped * Vec3f(grid_density));

    const int probe_index_at_point = (int(diff_units.x) * int(grid_density.y) * int(grid_density.z))
        + (int(diff_units.y) * int(grid_density.z))
        + int(diff_units.z);

    EnvProbeGridIndex calculated_probe_index = invalid_probe_index;

    if (probe_index_at_point >= 0 && uint32(probe_index_at_point) < max_bound_ambient_probes)
    {
        calculated_probe_index = EnvProbeGridIndex(
            Vec3u { uint32(diff_units.x), uint32(diff_units.y), uint32(diff_units.z) },
            grid_density);
    }

    return calculated_probe_index;
}

#pragma endregion Helpers

#pragma region Uniform buffer structs

struct LightFieldUniforms
{
    Vec4u image_dimensions;
    Vec4u probe_grid_position;
    Vec4u dimension_per_probe;
    Vec4u probe_offset_coord;

    uint32 num_bound_lights;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;

    uint32 light_indices[16];
};

#pragma endregion Uniform buffer structs

#pragma region Render commands

struct RENDER_COMMAND(SetElementInGlobalDescriptorSet)
    : RenderCommand
{
    Name set_name;
    Name element_name;
    DescriptorSetElement::ValueType value;

    RENDER_COMMAND(SetElementInGlobalDescriptorSet)(
        Name set_name,
        Name element_name,
        DescriptorSetElement::ValueType value)
        : set_name(set_name),
          element_name(element_name),
          value(std::move(value))
    {
    }

    virtual ~RENDER_COMMAND(SetElementInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            if (value.Is<GpuBufferRef>())
            {
                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(set_name, frame_index)->SetElement(element_name, value.Get<GpuBufferRef>());
            }
            else if (value.Is<ImageViewRef>())
            {
                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(set_name, frame_index)->SetElement(element_name, value.Get<ImageViewRef>());
            }
            else
            {
                AssertThrowMsg(false, "Not implemented");
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region RenderEnvGrid

RenderEnvGrid::RenderEnvGrid(EnvGrid* env_grid)
    : m_env_grid(env_grid)
{
}

RenderEnvGrid::~RenderEnvGrid()
{
}

void RenderEnvGrid::SetProbeIndices(Array<uint32>&& indices)
{
    HYP_SCOPE;

    Execute([this, indices = std::move(indices)]()
        {
            for (uint32 i = 0; i < uint32(indices.Size()); i++)
            {
                m_env_grid->GetEnvProbeCollection().SetIndexOnRenderThread(i, indices[i]);
            }

            SetNeedsUpdate();
        });
}

void RenderEnvGrid::Initialize_Internal()
{
    HYP_SCOPE;
}

void RenderEnvGrid::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderEnvGrid::Update_Internal()
{
    HYP_SCOPE;
}

GpuBufferHolderBase* RenderEnvGrid::GetGpuBufferHolder() const
{
    return g_render_global_state->gpu_buffers[GRB_ENV_GRIDS];
}

#pragma endregion RenderEnvGrid

#pragma region EnvGridPassData

EnvGridPassData::~EnvGridPassData()
{
    SafeRelease(std::move(clear_sh));
    SafeRelease(std::move(compute_sh));
    SafeRelease(std::move(reduce_sh));
    SafeRelease(std::move(finalize_sh));

    SafeRelease(std::move(compute_irradiance));
    SafeRelease(std::move(compute_filtered_depth));
    SafeRelease(std::move(copy_border_texels));

    SafeRelease(std::move(voxelize_probe));
    SafeRelease(std::move(offset_voxel_grid));

    SafeRelease(std::move(sh_tiles_buffers));

    SafeRelease(std::move(compute_sh_descriptor_tables));

    SafeRelease(std::move(voxel_grid_mips));

    SafeRelease(std::move(generate_voxel_grid_mipmaps));
    SafeRelease(std::move(generate_voxel_grid_mipmaps_descriptor_tables));
}

#pragma endregion EnvGridPassData

#pragma region EnvGridRenderer

EnvGridRenderer::EnvGridRenderer()
{
}

EnvGridRenderer::~EnvGridRenderer()
{
}

void EnvGridRenderer::Initialize()
{
}

void EnvGridRenderer::Shutdown()
{
}

PassData* EnvGridRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    EnvGridPassDataExt* ext_casted = ext.AsType<EnvGridPassDataExt>();
    AssertDebug(ext_casted != nullptr, "EnvGridPassDataExt must be provided for EnvGridRenderer");
    AssertDebug(ext_casted->env_grid != nullptr);

    EnvGrid* env_grid = ext_casted->env_grid;

    EnvProbeCollection& env_probe_collection = env_grid->GetEnvProbeCollection();

    EnvGridPassData* pd = new EnvGridPassData;
    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetRenderResource().GetViewport();

    pd->current_probe_index = env_probe_collection.num_probes != 0 ? 0 : ~0u;

    if (env_grid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        CreateVoxelGridData(env_grid, *pd);
    }

    switch (env_grid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        CreateSphericalHarmonicsData(env_grid, *pd);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        CreateLightFieldData(env_grid, *pd);

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    return pd;
}

void EnvGridRenderer::CreateVoxelGridData(EnvGrid* env_grid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(env_grid != nullptr);

    const EnvGridOptions& options = env_grid->GetOptions();

    if (!(options.flags & EnvGridFlags::USE_VOXEL_GRID))
    {
        return;
    }

    const ViewOutputTarget& output_target = env_grid->GetView()->GetOutputTarget();
    AssertDebug(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebugMsg(framebuffer.IsValid(), "Framebuffer must be created before voxelizing probes");

    // Create shader, descriptor sets for voxelizing probes

    ShaderRef voxelize_probe_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { "MODE_VOXELIZE" } });
    ShaderRef offset_voxel_grid_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { "MODE_OFFSET" } });
    ShaderRef clear_voxels_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_ClearProbeVoxels"));

    AttachmentBase* color_attachment = framebuffer->GetAttachment(0);
    AttachmentBase* normals_attachment = framebuffer->GetAttachment(1);
    AttachmentBase* depth_attachment = framebuffer->GetAttachment(2);

    const DescriptorTableDeclaration& descriptor_table_decl = voxelize_probe_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        // create descriptor sets for depth pyramid generation.
        DescriptorSetRef descriptor_set = descriptor_table->GetDescriptorSet(NAME("VoxelizeProbeDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InColorImage"), color_attachment ? color_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("InNormalsImage"), normals_attachment ? normals_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("InDepthImage"), depth_attachment ? depth_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());

        descriptor_set->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());

        descriptor_set->SetElement(NAME("EnvGridBuffer"), 0, sizeof(EnvGridShaderData), g_render_global_state->gpu_buffers[GRB_ENV_GRIDS]->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("EnvProbesBuffer"), g_render_global_state->gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame_index));

        descriptor_set->SetElement(NAME("OutVoxelGridImage"), env_grid->GetVoxelGridTexture()->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptor_table);

    { // Compute shader to clear the voxel grid at a specific position
        pd.clear_voxels = g_render_backend->MakeComputePipeline(clear_voxels_shader, descriptor_table);
        DeferCreate(pd.clear_voxels);
    }

    { // Compute shader to voxelize a probe into voxel grid
        pd.voxelize_probe = g_render_backend->MakeComputePipeline(voxelize_probe_shader, descriptor_table);
        DeferCreate(pd.voxelize_probe);
    }

    { // Compute shader to 'offset' the voxel grid
        pd.offset_voxel_grid = g_render_backend->MakeComputePipeline(offset_voxel_grid_shader, descriptor_table);
        DeferCreate(pd.offset_voxel_grid);
    }

    { // Compute shader to generate mipmaps for voxel grid
        ShaderRef generate_voxel_grid_mipmaps_shader = g_shader_manager->GetOrCreate(NAME("VCTGenerateMipmap"));
        AssertThrow(generate_voxel_grid_mipmaps_shader.IsValid());

        const DescriptorTableDeclaration& generate_voxel_grid_mipmaps_descriptor_table_decl = generate_voxel_grid_mipmaps_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        const uint32 num_voxel_grid_mip_levels = env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->NumMipmaps();
        pd.voxel_grid_mips.Resize(num_voxel_grid_mip_levels);

        for (uint32 mip_level = 0; mip_level < num_voxel_grid_mip_levels; mip_level++)
        {
            pd.voxel_grid_mips[mip_level] = g_render_backend->MakeImageView(
                env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(),
                mip_level, 1,
                0, env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->NumFaces());

            DeferCreate(pd.voxel_grid_mips[mip_level]);

            // create descriptor sets for mip generation.
            DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&generate_voxel_grid_mipmaps_descriptor_table_decl);

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const DescriptorSetRef& mip_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapDescriptorSet"), frame_index);
                AssertThrow(mip_descriptor_set != nullptr);

                if (mip_level == 0)
                {
                    // first mip level -- input is the actual image
                    mip_descriptor_set->SetElement(NAME("InputTexture"), env_grid->GetVoxelGridTexture()->GetRenderResource().GetImageView());
                }
                else
                {
                    mip_descriptor_set->SetElement(NAME("InputTexture"), pd.voxel_grid_mips[mip_level - 1]);
                }

                mip_descriptor_set->SetElement(NAME("OutputTexture"), pd.voxel_grid_mips[mip_level]);
            }

            DeferCreate(descriptor_table);

            pd.generate_voxel_grid_mipmaps_descriptor_tables.PushBack(std::move(descriptor_table));
        }

        pd.generate_voxel_grid_mipmaps = g_render_backend->MakeComputePipeline(generate_voxel_grid_mipmaps_shader, pd.generate_voxel_grid_mipmaps_descriptor_tables[0]);
        DeferCreate(pd.generate_voxel_grid_mipmaps);
    }
}

void EnvGridRenderer::CreateSphericalHarmonicsData(EnvGrid* env_grid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(env_grid != nullptr);

    pd.sh_tiles_buffers.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        const SizeType size = sizeof(SHTile) * (sh_num_tiles.x >> i) * (sh_num_tiles.y >> i);
        pd.sh_tiles_buffers[i] = g_render_backend->MakeGpuBuffer(GpuBufferType::SSBO, size);

        DeferCreate(pd.sh_tiles_buffers[i]);
    }

    FixedArray<ShaderRef, 4> shaders = {
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), { { "MODE_CLEAR" } }),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), { { "MODE_BUILD_COEFFICIENTS" } }),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), { { "MODE_REDUCE" } }),
        g_shader_manager->GetOrCreate(NAME("ComputeSH"), { { "MODE_FINALIZE" } })
    };

    for (const ShaderRef& shader : shaders)
    {
        AssertThrow(shader.IsValid());
    }

    const DescriptorTableDeclaration& descriptor_table_decl = shaders[0]->GetCompiledShader()->GetDescriptorTableDeclaration();

    pd.compute_sh_descriptor_tables.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        pd.compute_sh_descriptor_tables[i] = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& compute_sh_descriptor_set = pd.compute_sh_descriptor_tables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame_index);
            AssertThrow(compute_sh_descriptor_set != nullptr);

            compute_sh_descriptor_set->SetElement(NAME("InColorCubemap"), g_render_global_state->PlaceholderData->DefaultCubemap->GetRenderResource().GetImageView());
            compute_sh_descriptor_set->SetElement(NAME("InNormalsCubemap"), g_render_global_state->PlaceholderData->DefaultCubemap->GetRenderResource().GetImageView());
            compute_sh_descriptor_set->SetElement(NAME("InDepthCubemap"), g_render_global_state->PlaceholderData->DefaultCubemap->GetRenderResource().GetImageView());
            compute_sh_descriptor_set->SetElement(NAME("InputSHTilesBuffer"), pd.sh_tiles_buffers[i]);

            if (i != sh_num_levels - 1)
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), pd.sh_tiles_buffers[i + 1]);
            }
            else
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), pd.sh_tiles_buffers[i]);
            }
        }

        DeferCreate(pd.compute_sh_descriptor_tables[i]);
    }

    pd.clear_sh = g_render_backend->MakeComputePipeline(shaders[0], pd.compute_sh_descriptor_tables[0]);
    DeferCreate(pd.clear_sh);

    pd.compute_sh = g_render_backend->MakeComputePipeline(shaders[1], pd.compute_sh_descriptor_tables[0]);
    DeferCreate(pd.compute_sh);

    pd.reduce_sh = g_render_backend->MakeComputePipeline(shaders[2], pd.compute_sh_descriptor_tables[0]);
    DeferCreate(pd.reduce_sh);

    pd.finalize_sh = g_render_backend->MakeComputePipeline(shaders[3], pd.compute_sh_descriptor_tables[0]);
    DeferCreate(pd.finalize_sh);
}

void EnvGridRenderer::CreateLightFieldData(EnvGrid* env_grid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(env_grid != nullptr);
    AssertDebug(env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const ViewOutputTarget& output_target = env_grid->GetView()->GetOutputTarget();
    AssertDebug(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const EnvGridOptions& options = env_grid->GetOptions();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GpuBufferRef light_field_uniforms = g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(LightFieldUniforms));
        DeferCreate(light_field_uniforms);

        pd.uniform_buffers.PushBack(std::move(light_field_uniforms));
    }

    ShaderRef compute_irradiance_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeIrradiance"));
    ShaderRef compute_filtered_depth_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeFilteredDepth"));
    ShaderRef copy_border_texels_shader = g_shader_manager->GetOrCreate(NAME("LightField_CopyBorderTexels"));

    Pair<ShaderRef, ComputePipelineRef&> shaders[] = {
        { compute_irradiance_shader, pd.compute_irradiance },
        { compute_filtered_depth_shader, pd.compute_filtered_depth },
        { copy_border_texels_shader, pd.copy_border_texels }
    };

    for (Pair<ShaderRef, ComputePipelineRef&>& pair : shaders)
    {
        ShaderRef& shader = pair.first;
        ComputePipelineRef& pipeline = pair.second;

        AssertThrow(shader.IsValid());

        const DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            DescriptorSetRef descriptor_set = descriptor_table->GetDescriptorSet(NAME("LightFieldProbeDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("UniformBuffer"), pd.uniform_buffers[frame_index]);

            descriptor_set->SetElement(NAME("InColorImage"), framebuffer->GetAttachment(0)->GetImageView());
            descriptor_set->SetElement(NAME("InNormalsImage"), framebuffer->GetAttachment(1)->GetImageView());
            descriptor_set->SetElement(NAME("InDepthImage"), framebuffer->GetAttachment(2)->GetImageView());
            descriptor_set->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());
            descriptor_set->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());
            descriptor_set->SetElement(NAME("OutColorImage"), env_grid->GetLightFieldIrradianceTexture()->GetRenderResource().GetImageView());
            descriptor_set->SetElement(NAME("OutDepthImage"), env_grid->GetLightFieldDepthTexture()->GetRenderResource().GetImageView());
        }

        DeferCreate(descriptor_table);

        pipeline = g_render_backend->MakeComputePipeline(shader, descriptor_table);
        DeferCreate(pipeline);
    }
}

void EnvGridRenderer::RenderFrame(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.env_grid != nullptr);

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);

    EnvGridPassDataExt ext;
    ext.env_grid = env_grid;

    EnvGridPassData* pd = static_cast<EnvGridPassData*>(FetchViewPassData(env_grid->GetView(), &ext));
    AssertDebug(pd != nullptr);

    RenderSetup rs = render_setup;
    rs.view = &env_grid->GetView()->GetRenderResource();
    rs.pass_data = pd;

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(env_grid->GetView());

    /// FIXME: Not thread safe; use render proxy
    const BoundingBox grid_aabb = env_grid->GetAABB();

    if (!grid_aabb.IsValid() || !grid_aabb.IsFinite())
    {
        HYP_LOG(EnvGrid, Warning, "EnvGrid AABB is invalid or not finite - skipping rendering");

        return;
    }

    const EnvGridOptions& options = env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = env_grid->GetEnvProbeCollection();

    // Debug draw
    if (options.flags & EnvGridFlags::DEBUG_DISPLAY_PROBES)
    {
        for (uint32 index = 0; index < env_probe_collection.num_probes; index++)
        {
            const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(index);

            if (!probe.IsValid())
            {
                continue;
            }

            g_engine->GetDebugDrawer()->AmbientProbe(
                probe->GetRenderResource().GetBufferData().world_position.GetXYZ(),
                0.25f,
                *probe);
        }
    }

    // render enqueued probes
    while (pd->next_render_indices.Any())
    {
        RenderProbe(frame, rs, pd->next_render_indices.Pop());
    }

    if (env_probe_collection.num_probes != 0)
    {
        // update probe positions in grid, choose next to render.
        AssertThrow(pd->current_probe_index < env_probe_collection.num_probes);

        // const Vec3f &camera_position = camera_resource_handle->GetBufferData().camera_position.GetXYZ();

        Array<Pair<uint32, float>> indices_distances;
        indices_distances.Reserve(16);

        for (uint32 i = 0; i < env_probe_collection.num_probes; i++)
        {
            const uint32 index = (pd->current_probe_index + i) % env_probe_collection.num_probes;
            const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeOnRenderThread(index);

            if (probe.IsValid() && probe->NeedsRender())
            {
                indices_distances.PushBack({
                    index,
                    0 // probe->GetRenderResource().GetBufferData().world_position.GetXYZ().Distance(camera_position)
                });
            }
        }

        // std::sort(indices_distances.Begin(), indices_distances.End(), [](const auto &lhs, const auto &rhs) {
        //     return lhs.second < rhs.second;
        // });

        if (indices_distances.Any())
        {
            for (const auto& it : indices_distances)
            {
                const uint32 found_index = it.first;
                const uint32 indirect_index = env_probe_collection.GetIndexOnRenderThread(found_index);

                const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(indirect_index);
                AssertThrow(probe.IsValid());

                const Vec3f world_position = probe->GetRenderResource().GetBufferData().world_position.GetXYZ();

                const EnvProbeGridIndex binding_index = GetProbeBindingIndex(world_position, grid_aabb, options.density);

                if (binding_index != invalid_probe_index)
                {
                    if (pd->next_render_indices.Size() < max_queued_probes_for_render)
                    {
                        const Vec4i position_in_grid = Vec4i {
                            int32(indirect_index % options.density.x),
                            int32((indirect_index % (options.density.x * options.density.y)) / options.density.x),
                            int32(indirect_index / (options.density.x * options.density.y)),
                            int32(indirect_index)
                        };

                        probe->GetRenderResource().SetPositionInGrid(position_in_grid);

                        // render this probe in the next frame, since the data will have been updated on the gpu on start of the frame
                        pd->next_render_indices.Push(indirect_index);

                        pd->current_probe_index = (found_index + 1) % env_probe_collection.num_probes;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    HYP_LOG(EnvGrid, Warning, "EnvProbe {} out of range of max bound env probes (position: {}, world position: {}",
                        probe->Id(), binding_index.position, world_position);
                }

                probe->SetNeedsRender(false);
            }
        }
    }
}

void EnvGridRenderer::RenderProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);

    View* view = env_grid->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);

    const EnvGridOptions& options = env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = env_grid->GetEnvProbeCollection();

    const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());

    {
        RenderSetup rs = render_setup;
        rs.env_probe = probe;

        RenderCollector::ExecuteDrawCalls(frame, rs, rpl, (1u << RB_OPAQUE));
    }

    switch (env_grid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        ComputeEnvProbeIrradiance_SphericalHarmonics(frame, render_setup, probe);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        ComputeEnvProbeIrradiance_LightField(frame, render_setup, probe);

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    if (options.flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        VoxelizeProbe(frame, render_setup, probe_index);
    }

    probe->SetNeedsRender(false);
}

void EnvGridRenderer::ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    AssertDebug(probe.IsValid());

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);
    AssertDebug(env_grid->GetEnvGridType() == ENV_GRID_TYPE_SH);

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    EnvGridPassData* pd = static_cast<EnvGridPassData*>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& output_target = view->GetOutputTarget();

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    const EnvGridOptions& options = env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = env_grid->GetEnvProbeCollection();

    const uint32 grid_slot = probe->m_grid_slot;
    AssertThrow(grid_slot != ~0u);

    AttachmentBase* color_attachment = framebuffer->GetAttachment(0);
    AttachmentBase* normals_attachment = framebuffer->GetAttachment(1);
    AttachmentBase* depth_attachment = framebuffer->GetAttachment(2);

    const Vec2u cubemap_dimensions = color_attachment->GetImage()->GetExtent().GetXY();
    AssertThrow(cubemap_dimensions.Volume() > 0);

    struct
    {
        Vec4u probe_grid_position;
        Vec4u cubemap_dimensions;
        Vec4u level_dimensions;
        Vec4f world_position;
        uint32 env_probe_index;
    } push_constants;

    push_constants.env_probe_index = RenderApi_RetrieveResourceBinding(probe);

    push_constants.probe_grid_position = {
        grid_slot % options.density.x,
        (grid_slot % (options.density.x * options.density.y)) / options.density.x,
        grid_slot / (options.density.x * options.density.y),
        grid_slot
    };

    push_constants.cubemap_dimensions = Vec4u { cubemap_dimensions, 0, 0 };

    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    for (const DescriptorTableRef& descriptor_set_ref : pd->compute_sh_descriptor_tables)
    {
        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InColorCubemap"), framebuffer->GetAttachment(0)->GetImageView());

        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InNormalsCubemap"), framebuffer->GetAttachment(1)->GetImageView());

        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InDepthCubemap"), framebuffer->GetAttachment(2)->GetImageView());

        descriptor_set_ref->Update(frame->GetFrameIndex());
    }

    pd->clear_sh->SetPushConstants(&push_constants, sizeof(push_constants));
    pd->compute_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    CmdList& async_compute_command_list = g_render_backend->GetAsyncCompute()->GetCommandList();

    async_compute_command_list.Add<InsertBarrier>(pd->sh_tiles_buffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        pd->compute_sh_descriptor_tables[0],
        pd->clear_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pd->clear_sh);
    async_compute_command_list.Add<DispatchCompute>(pd->clear_sh, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(pd->sh_tiles_buffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        pd->compute_sh_descriptor_tables[0],
        pd->compute_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pd->compute_sh);
    async_compute_command_list.Add<DispatchCompute>(pd->compute_sh, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (sh_parallel_reduce)
    {
        for (uint32 i = 1; i < sh_num_levels; i++)
        {
            async_compute_command_list.Add<InsertBarrier>(pd->sh_tiles_buffers[i - 1], RS_UNORDERED_ACCESS, SMT_COMPUTE);

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

            pd->reduce_sh->SetPushConstants(&push_constants, sizeof(push_constants));

            async_compute_command_list.Add<BindDescriptorTable>(
                pd->compute_sh_descriptor_tables[i - 1],
                pd->reduce_sh,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                            { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light, 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
                frame->GetFrameIndex());

            async_compute_command_list.Add<BindComputePipeline>(pd->reduce_sh);
            async_compute_command_list.Add<DispatchCompute>(pd->reduce_sh, Vec3u { 1, (next_dimensions.x + 3) / 4, (next_dimensions.y + 3) / 4 });
        }
    }

    const uint32 finalize_sh_buffer_index = sh_parallel_reduce ? sh_num_levels - 1 : 0;

    // Finalize - build into final buffer
    async_compute_command_list.Add<InsertBarrier>(pd->sh_tiles_buffers[finalize_sh_buffer_index], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    pd->finalize_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    async_compute_command_list.Add<BindDescriptorTable>(
        pd->compute_sh_descriptor_tables[finalize_sh_buffer_index],
        pd->finalize_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pd->finalize_sh);
    async_compute_command_list.Add<DispatchCompute>(pd->finalize_sh, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    DelegateHandler* delegate_handle = new DelegateHandler();
    *delegate_handle = frame->OnFrameEnd.Bind(
        [resource_handle = TResourceHandle<RenderEnvProbe>(probe->GetRenderResource()), delegate_handle](FrameBase* frame)
        {
            HYP_NAMED_SCOPE("RenderEnvGrid::ComputeEnvProbeIrradiance_SphericalHarmonics - Buffer readback");

            EnvProbeShaderData readback_buffer;

            g_render_global_state->gpu_buffers[GRB_ENV_PROBES]->ReadbackElement(frame->GetFrameIndex(), resource_handle->GetBufferIndex(), &readback_buffer);

            // Log out SH data
            HYP_LOG(EnvGrid, Info, "EnvProbe {} SH data:\n\t{}\n\t{}\n\t{}\n\t{}\n",
                resource_handle->GetEnvProbe()->Id(),
                readback_buffer.sh.values[0],
                readback_buffer.sh.values[1],
                readback_buffer.sh.values[2],
                readback_buffer.sh.values[3]);

            resource_handle->SetSphericalHarmonics(readback_buffer.sh);

            delete delegate_handle;
        });
}

void EnvGridRenderer::ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);
    AssertDebug(env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    EnvGridPassData* pd = static_cast<EnvGridPassData*>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& output_target = view->GetOutputTarget();

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);

    RenderProxyEnvGrid* proxy = static_cast<RenderProxyEnvGrid*>(RenderApi_GetRenderProxy(env_grid->Id()));
    AssertThrow(proxy != nullptr, "EnvGrid render proxy not found!");

    const Vec2i irradiance_octahedron_size = proxy->buffer_data.irradiance_octahedron_size;

    const EnvGridOptions& options = env_grid->GetOptions();
    const uint32 probe_index = probe->m_grid_slot;

    { // Update uniform buffer

        LightFieldUniforms uniforms;
        Memory::MemSet(&uniforms, 0, sizeof(uniforms));

        uniforms.image_dimensions = Vec4u { env_grid->GetLightFieldIrradianceTexture()->GetExtent(), 0 };

        uniforms.probe_grid_position = {
            probe_index % options.density.x,
            (probe_index % (options.density.x * options.density.y)) / options.density.x,
            probe_index / (options.density.x * options.density.y),
            RenderApi_RetrieveResourceBinding(probe)
        };

        uniforms.dimension_per_probe = {
            uint32(irradiance_octahedron_size.x),
            uint32(irradiance_octahedron_size.y),
            0, 0
        };

        uniforms.probe_offset_coord = {
            (irradiance_octahedron_size.x + 2) * (uniforms.probe_grid_position.x * options.density.y + uniforms.probe_grid_position.y) + 1,
            (irradiance_octahedron_size.y + 2) * uniforms.probe_grid_position.z + 1,
            0, 0
        };

        const uint32 max_bound_lights = ArraySize(uniforms.light_indices);
        uint32 num_bound_lights = 0;

        for (Light* light : rpl.lights)
        {
            const LightType light_type = light->GetLightType();

            if (light_type != LT_DIRECTIONAL && light_type != LT_POINT)
            {
                continue;
            }

            if (num_bound_lights >= max_bound_lights)
            {
                break;
            }

            uniforms.light_indices[num_bound_lights++] = light->GetRenderResource().GetBufferIndex();
        }

        uniforms.num_bound_lights = num_bound_lights;

        pd->uniform_buffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
    }

    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetLightFieldIrradianceTexture()->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);
    frame->GetCommandList().Add<BindComputePipeline>(pd->compute_irradiance);

    frame->GetCommandList().Add<BindDescriptorTable>(
        pd->compute_irradiance->GetDescriptorTable(),
        pd->compute_irradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(pd->compute_irradiance, Vec3u { uint32(irradiance_octahedron_size.x + 7) / 8, uint32(irradiance_octahedron_size.y + 7) / 8, 1 });

    frame->GetCommandList().Add<BindComputePipeline>(pd->compute_filtered_depth);

    frame->GetCommandList().Add<BindDescriptorTable>(
        pd->compute_filtered_depth->GetDescriptorTable(),
        pd->compute_filtered_depth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(pd->compute_filtered_depth, Vec3u { uint32(irradiance_octahedron_size.x + 7) / 8, uint32(irradiance_octahedron_size.y + 7) / 8, 1 });

    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetLightFieldDepthTexture()->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(pd->copy_border_texels);
    frame->GetCommandList().Add<BindDescriptorTable>(
        pd->copy_border_texels->GetDescriptorTable(),
        pd->copy_border_texels,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(pd->copy_border_texels, Vec3u { uint32((irradiance_octahedron_size.x * 4) + 255) / 256, 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetLightFieldIrradianceTexture()->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetLightFieldDepthTexture()->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
}

void EnvGridRenderer::OffsetVoxelGrid(FrameBase* frame, const RenderSetup& render_setup, Vec3i offset)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);
    AssertDebug(env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    EnvGridPassData* pd = static_cast<EnvGridPassData*>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    AssertThrow(env_grid->GetVoxelGridTexture().IsValid());

    struct
    {
        Vec4u probe_grid_position;
        Vec4u cubemap_dimensions;
        Vec4i offset;
    } push_constants;

    Memory::MemSet(&push_constants, 0, sizeof(push_constants));

    push_constants.offset = { offset.x, offset.y, offset.z, 0 };

    pd->offset_voxel_grid->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);
    frame->GetCommandList().Add<BindComputePipeline>(pd->offset_voxel_grid);

    frame->GetCommandList().Add<BindDescriptorTable>(
        pd->offset_voxel_grid->GetDescriptorTable(),
        pd->offset_voxel_grid,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(pd->offset_voxel_grid, (env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->GetExtent() + Vec3u(7)) / Vec3u(8));
    frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
}

void EnvGridRenderer::VoxelizeProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    EnvGrid* env_grid = render_setup.env_grid;
    AssertDebug(env_grid != nullptr);
    AssertDebug(env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    EnvGridPassData* pd = static_cast<EnvGridPassData*>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& output_target = view->GetOutputTarget();
    AssertDebug(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    const EnvGridOptions& options = env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = env_grid->GetEnvProbeCollection();

    AssertThrow(env_grid->GetVoxelGridTexture().IsValid());

    const Vec3u& voxel_grid_texture_extent = env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->GetExtent();

    // size of a probe in the voxel grid
    const Vec3u probe_voxel_extent = voxel_grid_texture_extent / options.density;

    const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());
    AssertThrow(probe->IsReady());

    const ImageRef& color_image = framebuffer->GetAttachment(0)->GetImage();
    const Vec3u cubemap_dimensions = color_image->GetExtent();

    struct
    {
        Vec4u probe_grid_position;
        Vec4u voxel_texture_dimensions;
        Vec4u cubemap_dimensions;
        Vec4f world_position;
    } push_constants;

    push_constants.probe_grid_position = {
        probe_index % options.density.x,
        (probe_index % (options.density.x * options.density.y)) / options.density.x,
        probe_index / (options.density.x * options.density.y),
        RenderApi_RetrieveResourceBinding(probe)
    };

    push_constants.voxel_texture_dimensions = Vec4u(voxel_grid_texture_extent, 0);
    push_constants.cubemap_dimensions = Vec4u(cubemap_dimensions, 0);
    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    frame->GetCommandList().Add<InsertBarrier>(color_image, RS_SHADER_RESOURCE);

    if (false)
    { // Clear our voxel grid at the start of each probe
        pd->clear_voxels->SetPushConstants(&push_constants, sizeof(push_constants));

        frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

        frame->GetCommandList().Add<BindComputePipeline>(pd->clear_voxels);

        frame->GetCommandList().Add<BindDescriptorTable>(
            pd->clear_voxels->GetDescriptorTable(),
            pd->clear_voxels,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(pd->clear_voxels, (probe_voxel_extent + Vec3u(7)) / Vec3u(8));
    }

    { // Voxelize probe
        pd->voxelize_probe->SetPushConstants(&push_constants, sizeof(push_constants));

        frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);
        frame->GetCommandList().Add<BindComputePipeline>(pd->voxelize_probe);

        frame->GetCommandList().Add<BindDescriptorTable>(
            pd->voxelize_probe->GetDescriptorTable(),
            pd->voxelize_probe,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(
            pd->voxelize_probe,
            Vec3u {
                (probe_voxel_extent.x + 31) / 32,
                (probe_voxel_extent.y + 31) / 32,
                (probe_voxel_extent.z + 31) / 32 });
    }

    { // Generate mipmaps for the voxel grid
        frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);

        const uint32 num_mip_levels = env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->NumMipmaps();

        const Vec3u voxel_image_extent = env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage()->GetExtent();
        Vec3u mip_extent = voxel_image_extent;

        struct
        {
            Vec4u mip_dimensions;
            Vec4u prev_mip_dimensions;
            uint32 mip_level;
        } push_constant_data;

        for (uint32 mip_level = 0; mip_level < num_mip_levels; mip_level++)
        {
            const Vec3u prev_mip_extent = mip_extent;

            mip_extent.x = MathUtil::Max(1u, voxel_image_extent.x >> mip_level);
            mip_extent.y = MathUtil::Max(1u, voxel_image_extent.y >> mip_level);
            mip_extent.z = MathUtil::Max(1u, voxel_image_extent.z >> mip_level);

            push_constant_data.mip_dimensions = Vec4u { mip_extent.x, mip_extent.y, mip_extent.z, 0 };
            push_constant_data.prev_mip_dimensions = Vec4u { prev_mip_extent.x, prev_mip_extent.y, prev_mip_extent.z, 0 };
            push_constant_data.mip_level = mip_level;

            if (mip_level != 0)
            {
                // put the mip into writeable state
                frame->GetCommandList().Add<InsertBarrier>(
                    env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(),
                    RS_UNORDERED_ACCESS,
                    ImageSubResource { .base_mip_level = mip_level });
            }

            frame->GetCommandList().Add<BindDescriptorTable>(
                pd->generate_voxel_grid_mipmaps_descriptor_tables[mip_level],
                pd->generate_voxel_grid_mipmaps,
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frame->GetFrameIndex());

            pd->generate_voxel_grid_mipmaps->SetPushConstants(&push_constant_data, sizeof(push_constant_data));

            frame->GetCommandList().Add<BindComputePipeline>(pd->generate_voxel_grid_mipmaps);

            frame->GetCommandList().Add<DispatchCompute>(pd->generate_voxel_grid_mipmaps, (mip_extent + Vec3u(7)) / Vec3u(8));

            // put this mip into readable state
            frame->GetCommandList().Add<InsertBarrier>(
                env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(),
                RS_SHADER_RESOURCE,
                ImageSubResource { .base_mip_level = mip_level });
        }

        // all mip levels have been transitioned into this state
        frame->GetCommandList().Add<InsertBarrier>(env_grid->GetVoxelGridTexture()->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
    }
}

#pragma endregion EnvGridRenderer

HYP_DESCRIPTOR_CBUFF(Global, EnvGridsBuffer, 1, sizeof(EnvGridShaderData), true);

} // namespace hyperion
