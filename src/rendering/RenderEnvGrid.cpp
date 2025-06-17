/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/AsyncCompute.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

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

static const InternalFormat ambient_probe_format = InternalFormat::RGBA8;

static const Vec3u voxel_grid_dimensions { 256, 256, 256 };
static const InternalFormat voxel_grid_format = InternalFormat::RGBA8;

static const Vec2u framebuffer_dimensions { 256, 256 };
static const EnvProbeGridIndex invalid_probe_index = EnvProbeGridIndex();

const InternalFormat light_field_color_format = InternalFormat::RGBA8;
const InternalFormat light_field_depth_format = InternalFormat::RG16F;
static const uint32 irradiance_octahedron_size = 8;

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
    : renderer::RenderCommand
{
    Name set_name;
    Name element_name;
    renderer::DescriptorSetElement::ValueType value;

    RENDER_COMMAND(SetElementInGlobalDescriptorSet)(
        Name set_name,
        Name element_name,
        renderer::DescriptorSetElement::ValueType value)
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
            if (value.Is<GPUBufferRef>())
            {
                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(set_name, frame_index)->SetElement(element_name, value.Get<GPUBufferRef>());
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
    : m_env_grid(env_grid),
      m_buffer_data {}
{
    for (uint32 index = 0; index < ArraySize(m_buffer_data.probe_indices); index++)
    {
        m_buffer_data.probe_indices[index] = invalid_probe_index.GetProbeIndex();
    }
    m_buffer_data.center = Vec4f(env_grid->GetAABB().GetCenter(), 1.0f);
    m_buffer_data.extent = Vec4f(env_grid->GetAABB().GetExtent(), 1.0f);
    m_buffer_data.aabb_max = Vec4f(env_grid->GetAABB().max, 1.0f);
    m_buffer_data.aabb_min = Vec4f(env_grid->GetAABB().min, 1.0f);
    m_buffer_data.density = { env_grid->GetOptions().density.x, env_grid->GetOptions().density.y, env_grid->GetOptions().density.z, 0 };
    m_buffer_data.voxel_grid_aabb_max = Vec4f(env_grid->GetAABB().max, 1.0f);
    m_buffer_data.voxel_grid_aabb_min = Vec4f(env_grid->GetAABB().min, 1.0f);

    CreateShader();
}

RenderEnvGrid::~RenderEnvGrid()
{
    SafeRelease(std::move(m_shader));
}

void RenderEnvGrid::SetBufferData(const EnvGridShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            SetNeedsUpdate();
        });
}

void RenderEnvGrid::SetAABB(const BoundingBox& aabb)
{
    HYP_SCOPE;

    Execute([this, aabb]()
        {
            m_buffer_data.center = Vec4f(aabb.GetCenter(), 1.0f);
            m_buffer_data.extent = Vec4f(aabb.GetExtent(), 1.0f);
            m_buffer_data.aabb_max = Vec4f(aabb.max, 1.0f);
            m_buffer_data.aabb_min = Vec4f(aabb.min, 1.0f);

            SetNeedsUpdate();
        });
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

void RenderEnvGrid::SetCameraResourceHandle(TResourceHandle<RenderCamera>&& render_camera)
{
    HYP_SCOPE;

    Execute([this, render_camera = std::move(render_camera)]()
        {
            if (m_render_camera)
            {
                m_render_camera->SetFramebuffer(nullptr);
            }

            m_render_camera = std::move(render_camera);

            if (m_render_camera)
            {
                m_render_camera->SetFramebuffer(m_framebuffer);
            }
        });
}

void RenderEnvGrid::SetSceneResourceHandle(TResourceHandle<RenderScene>&& render_scene)
{
    HYP_SCOPE;

    Execute([this, render_scene = std::move(render_scene)]()
        {
            m_render_scene = std::move(render_scene);
        });
}

void RenderEnvGrid::SetViewResourceHandle(TResourceHandle<RenderView>&& render_view)
{
    HYP_SCOPE;

    Execute([this, render_view = std::move(render_view)]()
        {
            m_render_view = std::move(render_view);
        });
}

void RenderEnvGrid::CreateShader()
{
    HYP_SCOPE;

    ShaderProperties shader_properties(static_mesh_vertex_attributes, { "WRITE_NORMALS", "WRITE_MOMENTS" });

    m_shader = g_shader_manager->GetOrCreate(
        NAME("RenderToCubemap"),
        shader_properties);

    AssertThrow(m_shader.IsValid());
}

void RenderEnvGrid::CreateFramebuffer()
{
    HYP_SCOPE;

    m_framebuffer = g_rendering_api->MakeFramebuffer(framebuffer_dimensions, 6);

    m_framebuffer->AddAttachment(
        0,
        ambient_probe_format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    // Normals
    m_framebuffer->AddAttachment(
        1,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    // Distance Moments
    AttachmentRef moments_attachment = m_framebuffer->AddAttachment(
        2,
        InternalFormat::RG16F,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    // Set clear color for moments to be infinity
    moments_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

    m_framebuffer->AddAttachment(
        3,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    DeferCreate(m_framebuffer);

    if (m_render_camera)
    {
        m_render_camera->SetFramebuffer(m_framebuffer);
    }
}

void RenderEnvGrid::Initialize_Internal()
{
    HYP_SCOPE;

    CreateFramebuffer();

    if (m_env_grid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        CreateVoxelGridData();
    }

    switch (m_env_grid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        CreateSphericalHarmonicsData();

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        CreateLightFieldData();

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    UpdateBufferData();
}

void RenderEnvGrid::Destroy_Internal()
{
    HYP_SCOPE;

    if (m_voxel_grid_texture.IsValid())
    {
        PUSH_RENDER_COMMAND(
            SetElementInGlobalDescriptorSet,
            NAME("Global"),
            NAME("VoxelGridTexture"),
            g_render_global_state->PlaceholderData->GetImageView3D1x1x1R8());

        m_voxel_grid_texture.Reset();
    }

    if (m_render_camera)
    {
        m_render_camera->SetFramebuffer(nullptr);
    }

    m_render_camera.Reset();
    m_render_scene.Reset();
    m_render_view.Reset();

    SafeRelease(std::move(m_framebuffer));

    SafeRelease(std::move(m_clear_sh));
    SafeRelease(std::move(m_compute_sh));
    SafeRelease(std::move(m_reduce_sh));
    SafeRelease(std::move(m_finalize_sh));

    SafeRelease(std::move(m_voxelize_probe));
    SafeRelease(std::move(m_offset_voxel_grid));

    SafeRelease(std::move(m_sh_tiles_buffers));

    SafeRelease(std::move(m_compute_sh_descriptor_tables));

    SafeRelease(std::move(m_voxel_grid_mips));

    SafeRelease(std::move(m_generate_voxel_grid_mipmaps));
    SafeRelease(std::move(m_generate_voxel_grid_mipmaps_descriptor_tables));
}

void RenderEnvGrid::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderEnvGrid::CreateVoxelGridData()
{
    HYP_SCOPE;

    const EnvGridOptions& options = m_env_grid->GetOptions();

    if (!(options.flags & EnvGridFlags::USE_VOXEL_GRID))
    {
        return;
    }

    // Create our voxel grid texture
    m_voxel_grid_texture = CreateObject<Texture>(
        TextureDesc {
            ImageType::TEXTURE_TYPE_3D,
            voxel_grid_format,
            voxel_grid_dimensions,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_voxel_grid_texture);

    m_voxel_grid_texture->SetPersistentRenderResourceEnabled(true);

    // Set our voxel grid texture in the global descriptor set so we can use it in shaders
    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Global"),
        NAME("VoxelGridTexture"),
        m_voxel_grid_texture->GetRenderResource().GetImageView());

    // Create shader, descriptor sets for voxelizing probes
    AssertThrowMsg(m_framebuffer.IsValid(), "Framebuffer must be created before voxelizing probes");

    ShaderRef voxelize_probe_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { "MODE_VOXELIZE" } });
    ShaderRef offset_voxel_grid_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { "MODE_OFFSET" } });
    ShaderRef clear_voxels_shader = g_shader_manager->GetOrCreate(NAME("EnvProbe_ClearProbeVoxels"));

    AttachmentBase* color_attachment = m_framebuffer->GetAttachment(0);
    AttachmentBase* normals_attachment = m_framebuffer->GetAttachment(1);
    AttachmentBase* depth_attachment = m_framebuffer->GetAttachment(2);

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = voxelize_probe_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

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

        descriptor_set->SetElement(NAME("EnvGridBuffer"), 0, sizeof(EnvGridShaderData), g_render_global_state->EnvGrids->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("EnvProbesBuffer"), g_render_global_state->EnvProbes->GetBuffer(frame_index));

        descriptor_set->SetElement(NAME("OutVoxelGridImage"), m_voxel_grid_texture->GetRenderResource().GetImageView());

        AssertThrow(m_voxel_grid_texture->GetRenderResource().GetImageView() != nullptr);
    }

    DeferCreate(descriptor_table);

    { // Compute shader to clear the voxel grid at a specific position
        m_clear_voxels = g_rendering_api->MakeComputePipeline(
            clear_voxels_shader,
            descriptor_table);

        DeferCreate(m_clear_voxels);
    }

    { // Compute shader to voxelize a probe into voxel grid
        m_voxelize_probe = g_rendering_api->MakeComputePipeline(
            voxelize_probe_shader,
            descriptor_table);

        DeferCreate(m_voxelize_probe);
    }

    { // Compute shader to 'offset' the voxel grid
        m_offset_voxel_grid = g_rendering_api->MakeComputePipeline(
            offset_voxel_grid_shader,
            descriptor_table);

        DeferCreate(m_offset_voxel_grid);
    }

    { // Compute shader to generate mipmaps for voxel grid
        ShaderRef generate_voxel_grid_mipmaps_shader = g_shader_manager->GetOrCreate(NAME("VCTGenerateMipmap"));
        AssertThrow(generate_voxel_grid_mipmaps_shader.IsValid());

        const renderer::DescriptorTableDeclaration& generate_voxel_grid_mipmaps_descriptor_table_decl = generate_voxel_grid_mipmaps_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        const uint32 num_voxel_grid_mip_levels = m_voxel_grid_texture->GetRenderResource().GetImage()->NumMipmaps();
        m_voxel_grid_mips.Resize(num_voxel_grid_mip_levels);

        for (uint32 mip_level = 0; mip_level < num_voxel_grid_mip_levels; mip_level++)
        {
            m_voxel_grid_mips[mip_level] = g_rendering_api->MakeImageView(
                m_voxel_grid_texture->GetRenderResource().GetImage(),
                mip_level, 1,
                0, m_voxel_grid_texture->GetRenderResource().GetImage()->NumFaces());

            DeferCreate(m_voxel_grid_mips[mip_level]);

            // create descriptor sets for mip generation.
            DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&generate_voxel_grid_mipmaps_descriptor_table_decl);

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const DescriptorSetRef& mip_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapDescriptorSet"), frame_index);
                AssertThrow(mip_descriptor_set != nullptr);

                if (mip_level == 0)
                {
                    // first mip level -- input is the actual image
                    mip_descriptor_set->SetElement(NAME("InputTexture"), m_voxel_grid_texture->GetRenderResource().GetImageView());
                }
                else
                {
                    mip_descriptor_set->SetElement(NAME("InputTexture"), m_voxel_grid_mips[mip_level - 1]);
                }

                mip_descriptor_set->SetElement(NAME("OutputTexture"), m_voxel_grid_mips[mip_level]);
            }

            DeferCreate(descriptor_table);

            m_generate_voxel_grid_mipmaps_descriptor_tables.PushBack(std::move(descriptor_table));
        }

        m_generate_voxel_grid_mipmaps = g_rendering_api->MakeComputePipeline(
            generate_voxel_grid_mipmaps_shader,
            m_generate_voxel_grid_mipmaps_descriptor_tables[0]);

        DeferCreate(m_generate_voxel_grid_mipmaps);
    }
}

void RenderEnvGrid::CreateSphericalHarmonicsData()
{
    HYP_SCOPE;

    m_sh_tiles_buffers.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        const SizeType size = sizeof(SHTile) * (sh_num_tiles.x >> i) * (sh_num_tiles.y >> i);
        m_sh_tiles_buffers[i] = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, size);

        DeferCreate(m_sh_tiles_buffers[i]);
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

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = shaders[0]->GetCompiledShader()->GetDescriptorTableDeclaration();

    m_compute_sh_descriptor_tables.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        m_compute_sh_descriptor_tables[i] = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& compute_sh_descriptor_set = m_compute_sh_descriptor_tables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame_index);
            AssertThrow(compute_sh_descriptor_set != nullptr);

            compute_sh_descriptor_set->SetElement(NAME("InColorCubemap"), g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InNormalsCubemap"), g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InDepthCubemap"), g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InputSHTilesBuffer"), m_sh_tiles_buffers[i]);

            if (i != sh_num_levels - 1)
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), m_sh_tiles_buffers[i + 1]);
            }
            else
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), m_sh_tiles_buffers[i]);
            }
        }

        DeferCreate(m_compute_sh_descriptor_tables[i]);
    }

    m_clear_sh = g_rendering_api->MakeComputePipeline(
        shaders[0],
        m_compute_sh_descriptor_tables[0]);

    DeferCreate(m_clear_sh);

    m_compute_sh = g_rendering_api->MakeComputePipeline(
        shaders[1],
        m_compute_sh_descriptor_tables[0]);

    DeferCreate(m_compute_sh);

    m_reduce_sh = g_rendering_api->MakeComputePipeline(
        shaders[2],
        m_compute_sh_descriptor_tables[0]);

    DeferCreate(m_reduce_sh);

    m_finalize_sh = g_rendering_api->MakeComputePipeline(
        shaders[3],
        m_compute_sh_descriptor_tables[0]);

    DeferCreate(m_finalize_sh);
}

void RenderEnvGrid::CreateLightFieldData()
{
    HYP_SCOPE;

    AssertThrow(m_env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const EnvGridOptions& options = m_env_grid->GetOptions();

    m_irradiance_texture = CreateObject<Texture>(
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            light_field_color_format,
            Vec3u {
                (irradiance_octahedron_size + 2) * options.density.x * options.density.y + 2,
                (irradiance_octahedron_size + 2) * options.density.z + 2,
                1 },
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_irradiance_texture);

    m_irradiance_texture->SetPersistentRenderResourceEnabled(true);

    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Global"),
        NAME("LightFieldColorTexture"),
        m_irradiance_texture->GetRenderResource().GetImageView());

    m_depth_texture = CreateObject<Texture>(
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            light_field_depth_format,
            Vec3u {
                (irradiance_octahedron_size + 2) * options.density.x * options.density.y + 2,
                (irradiance_octahedron_size + 2) * options.density.z + 2,
                1 },
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_depth_texture);

    m_depth_texture->SetPersistentRenderResourceEnabled(true);

    PUSH_RENDER_COMMAND(
        SetElementInGlobalDescriptorSet,
        NAME("Global"),
        NAME("LightFieldDepthTexture"),
        m_depth_texture->GetRenderResource().GetImageView());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GPUBufferRef light_field_uniforms = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(LightFieldUniforms));

        DeferCreate(light_field_uniforms);

        m_uniform_buffers.PushBack(std::move(light_field_uniforms));
    }

    ShaderRef compute_irradiance_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeIrradiance"));
    ShaderRef compute_filtered_depth_shader = g_shader_manager->GetOrCreate(NAME("LightField_ComputeFilteredDepth"));
    ShaderRef copy_border_texels_shader = g_shader_manager->GetOrCreate(NAME("LightField_CopyBorderTexels"));

    Pair<ShaderRef, ComputePipelineRef&> shaders[] = {
        { compute_irradiance_shader, m_compute_irradiance },
        { compute_filtered_depth_shader, m_compute_filtered_depth },
        { copy_border_texels_shader, m_copy_border_texels }
    };

    for (Pair<ShaderRef, ComputePipelineRef&>& pair : shaders)
    {
        ShaderRef& shader = pair.first;
        ComputePipelineRef& pipeline = pair.second;

        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            DescriptorSetRef descriptor_set = descriptor_table->GetDescriptorSet(NAME("LightFieldProbeDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffers[frame_index]);

            descriptor_set->SetElement(NAME("InColorImage"), m_framebuffer->GetAttachment(0)->GetImageView());
            descriptor_set->SetElement(NAME("InNormalsImage"), m_framebuffer->GetAttachment(1)->GetImageView());
            descriptor_set->SetElement(NAME("InDepthImage"), m_framebuffer->GetAttachment(2)->GetImageView());
            descriptor_set->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());
            descriptor_set->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());
            descriptor_set->SetElement(NAME("OutColorImage"), m_irradiance_texture->GetRenderResource().GetImageView());
            descriptor_set->SetElement(NAME("OutDepthImage"), m_depth_texture->GetRenderResource().GetImageView());
        }

        DeferCreate(descriptor_table);

        pipeline = g_rendering_api->MakeComputePipeline(shader, descriptor_table);
        DeferCreate(pipeline);
    }
}

GPUBufferHolderBase* RenderEnvGrid::GetGPUBufferHolder() const
{
    return g_render_global_state->EnvGrids;
}

void RenderEnvGrid::UpdateBufferData()
{
    HYP_SCOPE;

    m_buffer_data.light_field_image_dimensions = m_irradiance_texture.IsValid()
        ? Vec2i(m_irradiance_texture->GetExtent().GetXY())
        : Vec2i::Zero();

    m_buffer_data.irradiance_octahedron_size = Vec2i(irradiance_octahedron_size, irradiance_octahedron_size);

    for (uint32 index = 0; index < ArraySize(m_buffer_data.probe_indices); index++)
    {
        const Handle<EnvProbe>& probe = m_env_grid->GetEnvProbeCollection().GetEnvProbeOnRenderThread(index);
        AssertThrow(probe.IsValid());

        m_buffer_data.probe_indices[index] = probe->GetRenderResource().GetBufferIndex();
    }

    *static_cast<EnvGridShaderData*>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderEnvGrid::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const BoundingBox grid_aabb = BoundingBox(
        m_buffer_data.aabb_min.GetXYZ(),
        m_buffer_data.aabb_max.GetXYZ());

    if (!grid_aabb.IsValid() || !grid_aabb.IsFinite())
    {
        HYP_LOG(EnvGrid, Warning, "EnvGrid AABB is invalid or not finite - skipping rendering");

        return;
    }

    const EnvGridOptions& options = m_env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = m_env_grid->GetEnvProbeCollection();

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
    while (m_next_render_indices.Any())
    {
        const RenderSetup new_render_setup { render_setup.world, m_render_view.Get() };

        RenderProbe(frame, new_render_setup, m_next_render_indices.Pop());
    }

    if (env_probe_collection.num_probes != 0)
    {
        // update probe positions in grid, choose next to render.
        AssertThrow(m_current_probe_index < env_probe_collection.num_probes);

        // const Vec3f &camera_position = camera_resource_handle->GetBufferData().camera_position.GetXYZ();

        Array<Pair<uint32, float>> indices_distances;
        indices_distances.Reserve(16);

        for (uint32 i = 0; i < env_probe_collection.num_probes; i++)
        {
            const uint32 index = (m_current_probe_index + i) % env_probe_collection.num_probes;
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
                    if (m_next_render_indices.Size() < max_queued_probes_for_render)
                    {
                        const Vec4i position_in_grid = Vec4i {
                            int32(indirect_index % options.density.x),
                            int32((indirect_index % (options.density.x * options.density.y)) / options.density.x),
                            int32(indirect_index / (options.density.x * options.density.y)),
                            int32(indirect_index)
                        };

                        probe->GetRenderResource().SetPositionInGrid(position_in_grid);

                        // render this probe in the next frame, since the data will have been updated on the gpu on start of the frame
                        m_next_render_indices.Push(indirect_index);

                        m_current_probe_index = (found_index + 1) % env_probe_collection.num_probes;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    HYP_LOG(EnvGrid, Warning, "EnvProbe {} out of range of max bound env probes (position: {}, world position: {}",
                        probe->GetID(), binding_index.position, world_position);
                }

                probe->SetNeedsRender(false);
            }
        }
    }
}

void RenderEnvGrid::RenderProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());

    const EnvGridOptions& options = m_env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = m_env_grid->GetEnvProbeCollection();

    const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());

    // Bind a directional light
    RenderLight* render_light = nullptr;

    {
        auto& directional_lights = m_render_view->GetLights(LightType::DIRECTIONAL);

        if (directional_lights.Any())
        {
            render_light = directional_lights.Front();
            AssertDebug(render_light != nullptr);

            g_engine->GetRenderState()->SetActiveLight(TResourceHandle<RenderLight>(*render_light));
        }
    }

    RenderSetup new_render_setup = render_setup;
    new_render_setup.env_probe = &probe->GetRenderResource();

    m_render_view->GetRenderCollector().CollectDrawCalls(
        frame,
        new_render_setup,
        Bitset((1 << BUCKET_OPAQUE)));

    m_render_view->GetRenderCollector().ExecuteDrawCalls(
        frame,
        new_render_setup,
        Bitset((1 << BUCKET_OPAQUE)));

    if (render_light != nullptr)
    {
        g_engine->GetRenderState()->UnsetActiveLight();
    }

    // use sky as backdrop
    if (const Array<RenderEnvProbe*>& sky_env_probes = m_render_view->GetEnvProbes(EnvProbeType::SKY); sky_env_probes.Any())
    {
        new_render_setup.env_probe = sky_env_probes.Front();
    }
    else
    {
        new_render_setup.env_probe = nullptr;

        HYP_LOG(EnvGrid, Warning, "No sky EnvProbe found!");
    }

    switch (m_env_grid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        ComputeEnvProbeIrradiance_SphericalHarmonics(frame, new_render_setup, probe);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        ComputeEnvProbeIrradiance_LightField(frame, new_render_setup, probe);

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    if (options.flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        VoxelizeProbe(frame, probe_index);
    }

    probe->SetNeedsRender(false);
}

void RenderEnvGrid::ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    const EnvGridOptions& options = m_env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = m_env_grid->GetEnvProbeCollection();

    AssertThrow(m_env_grid->GetEnvGridType() == ENV_GRID_TYPE_SH);

    const uint32 grid_slot = probe->m_grid_slot;
    AssertThrow(grid_slot != ~0u);

    AttachmentBase* color_attachment = m_framebuffer->GetAttachment(0);
    AttachmentBase* normals_attachment = m_framebuffer->GetAttachment(1);
    AttachmentBase* depth_attachment = m_framebuffer->GetAttachment(2);

    // Bind a directional light
    RenderLight* render_light = nullptr;

    {
        auto& directional_lights = m_render_view->GetLights(LightType::DIRECTIONAL);

        if (directional_lights.Any())
        {
            render_light = directional_lights.Front();
        }
    }

    const Vec2u cubemap_dimensions = color_attachment->GetImage()->GetExtent().GetXY();

    struct
    {
        uint32 env_probe_index;
        Vec4u probe_grid_position;
        Vec4u cubemap_dimensions;
        Vec4u level_dimensions;
        Vec4f world_position;
    } push_constants;

    push_constants.env_probe_index = probe->GetRenderResource().GetBufferIndex();

    push_constants.probe_grid_position = {
        grid_slot % options.density.x,
        (grid_slot % (options.density.x * options.density.y)) / options.density.x,
        grid_slot / (options.density.x * options.density.y),
        grid_slot
    };

    push_constants.cubemap_dimensions = Vec4u { cubemap_dimensions, 0, 0 };

    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    for (const DescriptorTableRef& descriptor_set_ref : m_compute_sh_descriptor_tables)
    {
        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InColorCubemap"), color_attachment->GetImageView());

        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InNormalsCubemap"), normals_attachment->GetImageView());

        descriptor_set_ref->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InDepthCubemap"), depth_attachment->GetImageView());

        descriptor_set_ref->Update(frame->GetFrameIndex());
    }

    m_clear_sh->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    RHICommandList& async_compute_command_list = g_rendering_api->GetAsyncCompute()->GetCommandList();

    async_compute_command_list.Add<InsertBarrier>(
        m_sh_tiles_buffers[0],
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<InsertBarrier>(
        g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()),
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        m_compute_sh_descriptor_tables[0],
        m_clear_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(m_clear_sh);
    async_compute_command_list.Add<DispatchCompute>(m_clear_sh, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(
        m_sh_tiles_buffers[0],
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        m_compute_sh_descriptor_tables[0],
        m_compute_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(m_compute_sh);
    async_compute_command_list.Add<DispatchCompute>(m_compute_sh, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (sh_parallel_reduce)
    {
        for (uint32 i = 1; i < sh_num_levels; i++)
        {
            async_compute_command_list.Add<InsertBarrier>(
                m_sh_tiles_buffers[i - 1],
                renderer::ResourceState::UNORDERED_ACCESS,
                renderer::ShaderModuleType::COMPUTE);

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

            async_compute_command_list.Add<BindDescriptorTable>(
                m_compute_sh_descriptor_tables[i - 1],
                m_reduce_sh,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                            { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
                frame->GetFrameIndex());

            async_compute_command_list.Add<BindComputePipeline>(m_reduce_sh);
            async_compute_command_list.Add<DispatchCompute>(m_reduce_sh, Vec3u { 1, (next_dimensions.x + 3) / 4, (next_dimensions.y + 3) / 4 });
        }
    }

    const uint32 finalize_sh_buffer_index = sh_parallel_reduce ? sh_num_levels - 1 : 0;

    // Finalize - build into final buffer
    async_compute_command_list.Add<InsertBarrier>(
        m_sh_tiles_buffers[finalize_sh_buffer_index],
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<InsertBarrier>(
        g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()),
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    m_finalize_sh->SetPushConstants(&push_constants, sizeof(push_constants));

    async_compute_command_list.Add<BindDescriptorTable>(
        m_compute_sh_descriptor_tables[finalize_sh_buffer_index],
        m_finalize_sh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(m_finalize_sh);
    async_compute_command_list.Add<DispatchCompute>(m_finalize_sh, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(
        g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()),
        renderer::ResourceState::UNORDERED_ACCESS,
        renderer::ShaderModuleType::COMPUTE);

    DelegateHandler* delegate_handle = new DelegateHandler();
    *delegate_handle = frame->OnFrameEnd.Bind(
        [resource_handle = TResourceHandle<RenderEnvProbe>(probe->GetRenderResource()), delegate_handle](FrameBase* frame)
        {
            HYP_NAMED_SCOPE("RenderEnvGrid::ComputeEnvProbeIrradiance_SphericalHarmonics - Buffer readback");

            EnvProbeShaderData readback_buffer;

            g_render_global_state->EnvProbes->ReadbackElement(frame->GetFrameIndex(), resource_handle->GetBufferIndex(), &readback_buffer);

            resource_handle->SetSphericalHarmonics(readback_buffer.sh);

            delete delegate_handle;
        });
}

void RenderEnvGrid::ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    AssertThrow(m_env_grid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const EnvGridOptions& options = m_env_grid->GetOptions();

    const uint32 probe_index = probe->m_grid_slot;

    { // Update uniform buffer
        LightFieldUniforms uniforms;
        Memory::MemSet(&uniforms, 0, sizeof(uniforms));

        uniforms.image_dimensions = Vec4u { m_irradiance_texture->GetExtent(), 0 };

        uniforms.probe_grid_position = {
            probe_index % options.density.x,
            (probe_index % (options.density.x * options.density.y)) / options.density.x,
            probe_index / (options.density.x * options.density.y),
            probe->GetRenderResource().GetBufferIndex()
        };

        uniforms.dimension_per_probe = { irradiance_octahedron_size, irradiance_octahedron_size, 0, 0 };

        uniforms.probe_offset_coord = {
            (irradiance_octahedron_size + 2) * (uniforms.probe_grid_position.x * options.density.y + uniforms.probe_grid_position.y) + 1,
            (irradiance_octahedron_size + 2) * uniforms.probe_grid_position.z + 1,
            0, 0
        };

        const uint32 max_bound_lights = ArraySize(uniforms.light_indices);
        uint32 num_bound_lights = 0;

        for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++)
        {
            if (num_bound_lights >= max_bound_lights)
            {
                break;
            }

            for (const auto& it : m_render_view->GetLights(LightType(light_type)))
            {
                if (num_bound_lights >= max_bound_lights)
                {
                    break;
                }

                uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
            }
        }

        uniforms.num_bound_lights = num_bound_lights;

        m_uniform_buffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
    }

    frame->GetCommandList().Add<InsertBarrier>(
        m_irradiance_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(m_compute_irradiance);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_compute_irradiance->GetDescriptorTable(),
        m_compute_irradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_compute_irradiance,
        Vec3u {
            (irradiance_octahedron_size + 7) / 8,
            (irradiance_octahedron_size + 7) / 8,
            1 });

    frame->GetCommandList().Add<BindComputePipeline>(m_compute_filtered_depth);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_compute_filtered_depth->GetDescriptorTable(),
        m_compute_filtered_depth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_compute_filtered_depth,
        Vec3u {
            (irradiance_octahedron_size + 7) / 8,
            (irradiance_octahedron_size + 7) / 8,
            1 });

    frame->GetCommandList().Add<InsertBarrier>(
        m_depth_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(m_copy_border_texels);
    frame->GetCommandList().Add<BindDescriptorTable>(
        m_copy_border_texels->GetDescriptorTable(),
        m_copy_border_texels,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_copy_border_texels,
        Vec3u { ((irradiance_octahedron_size * 4) + 255) / 256, 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(
        m_irradiance_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::SHADER_RESOURCE);

    frame->GetCommandList().Add<InsertBarrier>(
        m_irradiance_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::SHADER_RESOURCE);
}

void RenderEnvGrid::OffsetVoxelGrid(FrameBase* frame, Vec3i offset)
{
    HYP_SCOPE;

    AssertThrow(m_voxel_grid_texture.IsValid());

    struct
    {
        Vec4u probe_grid_position;
        Vec4u cubemap_dimensions;
        Vec4i offset;
    } push_constants;

    Memory::MemSet(&push_constants, 0, sizeof(push_constants));

    push_constants.offset = { offset.x, offset.y, offset.z, 0 };

    m_offset_voxel_grid->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<InsertBarrier>(
        m_voxel_grid_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(m_offset_voxel_grid);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_offset_voxel_grid->GetDescriptorTable(),
        m_offset_voxel_grid,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_offset_voxel_grid,
        (m_voxel_grid_texture->GetRenderResource().GetImage()->GetExtent() + Vec3u(7)) / Vec3u(8));

    frame->GetCommandList().Add<InsertBarrier>(
        m_voxel_grid_texture->GetRenderResource().GetImage(),
        renderer::ResourceState::SHADER_RESOURCE);
}

void RenderEnvGrid::VoxelizeProbe(
    FrameBase* frame,
    uint32 probe_index)
{
    const EnvGridOptions& options = m_env_grid->GetOptions();
    const EnvProbeCollection& env_probe_collection = m_env_grid->GetEnvProbeCollection();

    AssertThrow(m_voxel_grid_texture.IsValid());

    const Vec3u& voxel_grid_texture_extent = m_voxel_grid_texture->GetRenderResource().GetImage()->GetExtent();

    // size of a probe in the voxel grid
    const Vec3u probe_voxel_extent = voxel_grid_texture_extent / options.density;

    const Handle<EnvProbe>& probe = env_probe_collection.GetEnvProbeDirect(probe_index);
    AssertThrow(probe.IsValid());
    AssertThrow(probe->IsReady());

    const ImageRef& color_image = m_framebuffer->GetAttachment(0)->GetImage();
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
        probe->GetRenderResource().GetBufferIndex()
    };

    push_constants.voxel_texture_dimensions = Vec4u(voxel_grid_texture_extent, 0);
    push_constants.cubemap_dimensions = Vec4u(cubemap_dimensions, 0);
    push_constants.world_position = probe->GetRenderResource().GetBufferData().world_position;

    frame->GetCommandList().Add<InsertBarrier>(
        color_image,
        renderer::ResourceState::SHADER_RESOURCE);

    if (false)
    { // Clear our voxel grid at the start of each probe
        m_clear_voxels->SetPushConstants(&push_constants, sizeof(push_constants));

        frame->GetCommandList().Add<InsertBarrier>(
            m_voxel_grid_texture->GetRenderResource().GetImage(),
            renderer::ResourceState::UNORDERED_ACCESS);

        frame->GetCommandList().Add<BindComputePipeline>(m_clear_voxels);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_clear_voxels->GetDescriptorTable(),
            m_clear_voxels,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(
            m_clear_voxels,
            (probe_voxel_extent + Vec3u(7)) / Vec3u(8));
    }

    { // Voxelize probe
        m_voxelize_probe->SetPushConstants(&push_constants, sizeof(push_constants));

        frame->GetCommandList().Add<InsertBarrier>(
            m_voxel_grid_texture->GetRenderResource().GetImage(),
            renderer::ResourceState::UNORDERED_ACCESS);

        frame->GetCommandList().Add<BindComputePipeline>(m_voxelize_probe);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_voxelize_probe->GetDescriptorTable(),
            m_voxelize_probe,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(m_buffer_index) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(
            m_voxelize_probe,
            Vec3u {
                (probe_voxel_extent.x + 31) / 32,
                (probe_voxel_extent.y + 31) / 32,
                (probe_voxel_extent.z + 31) / 32 });
    }

    { // Generate mipmaps for the voxel grid
        frame->GetCommandList().Add<InsertBarrier>(
            m_voxel_grid_texture->GetRenderResource().GetImage(),
            renderer::ResourceState::SHADER_RESOURCE);

        const uint32 num_mip_levels = m_voxel_grid_texture->GetRenderResource().GetImage()->NumMipmaps();

        const Vec3u voxel_image_extent = m_voxel_grid_texture->GetRenderResource().GetImage()->GetExtent();
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
                    m_voxel_grid_texture->GetRenderResource().GetImage(),
                    renderer::ResourceState::UNORDERED_ACCESS,
                    renderer::ImageSubResource { .base_mip_level = mip_level });
            }

            frame->GetCommandList().Add<BindDescriptorTable>(
                m_generate_voxel_grid_mipmaps_descriptor_tables[mip_level],
                m_generate_voxel_grid_mipmaps,
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frame->GetFrameIndex());

            m_generate_voxel_grid_mipmaps->SetPushConstants(&push_constant_data, sizeof(push_constant_data));

            frame->GetCommandList().Add<BindComputePipeline>(m_generate_voxel_grid_mipmaps);

            frame->GetCommandList().Add<DispatchCompute>(
                m_generate_voxel_grid_mipmaps,
                (mip_extent + Vec3u(7)) / Vec3u(8));

            // put this mip into readable state
            frame->GetCommandList().Add<InsertBarrier>(
                m_voxel_grid_texture->GetRenderResource().GetImage(),
                renderer::ResourceState::SHADER_RESOURCE,
                renderer::ImageSubResource { .base_mip_level = mip_level });
        }

        // all mip levels have been transitioned into this state
        frame->GetCommandList().Add<InsertBarrier>(
            m_voxel_grid_texture->GetRenderResource().GetImage(),
            renderer::ResourceState::SHADER_RESOURCE);
    }
}

#pragma endregion RenderEnvGrid

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Global, EnvGridsBuffer, 1, sizeof(EnvGridShaderData), true);

} // namespace renderer

} // namespace hyperion
