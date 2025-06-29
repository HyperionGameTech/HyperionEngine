/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

enum ProbeSystemUpdates : uint32
{
    PROBE_SYSTEM_UPDATES_NONE = 0x0,
    PROBE_SYSTEM_UPDATES_TLAS = 0x1
};

static constexpr TextureFormat ddgi_irradiance_format = TF_RGBA16F;
static constexpr TextureFormat ddgi_depth_format = TF_RG16F;

#pragma region Render commands

struct RENDER_COMMAND(SetDDGIDescriptors)
    : RenderCommand
{
    GpuBufferRef uniform_buffer;
    ImageViewRef irradiance_image_view;
    ImageViewRef depth_image_view;

    RENDER_COMMAND(SetDDGIDescriptors)(
        const GpuBufferRef& uniform_buffer,
        const ImageViewRef& irradiance_image_view,
        const ImageViewRef& depth_image_view)
        : uniform_buffer(uniform_buffer),
          irradiance_image_view(irradiance_image_view),
          depth_image_view(depth_image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), uniform_buffer);

            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), irradiance_image_view);

            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), depth_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDDGIDescriptors)
    : RenderCommand
{
    RENDER_COMMAND(UnsetDDGIDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(UnsetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        // remove result image from global descriptor set
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());

            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIUniformBuffer)
    : RenderCommand
{
    GpuBufferRef uniform_buffer;
    DDGIUniforms uniforms;

    RENDER_COMMAND(CreateDDGIUniformBuffer)(const GpuBufferRef& uniform_buffer, const DDGIUniforms& uniforms)
        : uniform_buffer(uniform_buffer),
          uniforms(uniforms)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create());
        uniform_buffer->Copy(sizeof(DDGIUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIRadianceBuffer)
    : RenderCommand
{
    GpuBufferRef radiance_buffer;
    DDGIInfo grid_info;

    RENDER_COMMAND(CreateDDGIRadianceBuffer)(const GpuBufferRef& radiance_buffer, const DDGIInfo& grid_info)
        : radiance_buffer(radiance_buffer),
          grid_info(grid_info)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIRadianceBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(radiance_buffer->Create());
        radiance_buffer->Memset(radiance_buffer->Size(), 0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

DDGI::DDGI(DDGIInfo&& grid_info)
    : m_grid_info(std::move(grid_info)),
      m_updates { PROBE_SYSTEM_UPDATES_NONE, PROBE_SYSTEM_UPDATES_NONE },
      m_time(0)
{
}

DDGI::~DDGI()
{
}

void DDGI::Init()
{
    const Vec3u grid = m_grid_info.NumProbesPerDimension();
    m_probes.Resize(m_grid_info.NumProbes());

    for (uint32 x = 0; x < grid.x; x++)
    {
        for (uint32 y = 0; y < grid.y; y++)
        {
            for (uint32 z = 0; z < grid.z; z++)
            {
                const uint32 index = x * grid.x * grid.y + y * grid.z + z;

                m_probes[index] = Probe {
                    (Vec3f { float(x), float(y), float(z) } - (Vec3f(m_grid_info.probe_border) * 0.5f)) * m_grid_info.probe_distance
                };
            }
        }
    }

    CreateStorageBuffers();
    CreateUniformBuffer();
    CreatePipelines();

    PUSH_RENDER_COMMAND(
        SetDDGIDescriptors,
        m_uniform_buffer,
        m_irradiance_image_view,
        m_depth_image_view);
}

void DDGI::Destroy()
{
    m_shader.Reset();

    SafeRelease(std::move(m_uniform_buffer));
    SafeRelease(std::move(m_radiance_buffer));
    SafeRelease(std::move(m_irradiance_image));
    SafeRelease(std::move(m_irradiance_image_view));
    SafeRelease(std::move(m_depth_image));
    SafeRelease(std::move(m_depth_image_view));
    SafeRelease(std::move(m_pipeline));
    SafeRelease(std::move(m_update_irradiance));
    SafeRelease(std::move(m_update_depth));
    SafeRelease(std::move(m_copy_border_texels_irradiance));
    SafeRelease(std::move(m_copy_border_texels_depth));

    PUSH_RENDER_COMMAND(UnsetDDGIDescriptors);

    HYP_SYNC_RENDER();
}

void DDGI::CreatePipelines()
{
    m_shader = g_shader_manager->GetOrCreate(NAME("DDGI"));
    AssertThrow(m_shader.IsValid());

    const DescriptorTableDeclaration& raytracing_pipeline_descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef raytracing_pipeline_descriptor_table = g_render_backend->MakeDescriptorTable(&raytracing_pipeline_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        AssertThrow(m_top_level_acceleration_structures[frame_index] != nullptr);

        const DescriptorSetRef& descriptor_set = raytracing_pipeline_descriptor_table->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);

        descriptor_set->SetElement(NAME("LightsBuffer"), g_render_global_state->gpu_buffers[GRB_LIGHTS]->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_render_global_state->gpu_buffers[GRB_MATERIALS]->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(NAME("DDGIUniforms"), m_uniform_buffer);
        descriptor_set->SetElement(NAME("ProbeRayData"), m_radiance_buffer);
    }

    DeferCreate(raytracing_pipeline_descriptor_table);

    // Create raytracing pipeline

    m_pipeline = g_render_backend->MakeRaytracingPipeline(
        m_shader,
        raytracing_pipeline_descriptor_table);

    DeferCreate(m_pipeline);

    ShaderRef update_irradiance_shader = g_shader_manager->GetOrCreate(NAME("RTProbeUpdateIrradiance"));
    ShaderRef update_depth_shader = g_shader_manager->GetOrCreate(NAME("RTProbeUpdateDepth"));
    ShaderRef copy_border_texels_irradiance_shader = g_shader_manager->GetOrCreate(NAME("RTCopyBorderTexelsIrradiance"));
    ShaderRef copy_border_texels_depth_shader = g_shader_manager->GetOrCreate(NAME("RTCopyBorderTexelsDepth"));

    Pair<ShaderRef, ComputePipelineRef&> shaders[] = {
        { update_irradiance_shader, m_update_irradiance },
        { update_depth_shader, m_update_depth },
        { copy_border_texels_irradiance_shader, m_copy_border_texels_irradiance },
        { copy_border_texels_depth_shader, m_copy_border_texels_depth }
    };

    for (const Pair<ShaderRef, ComputePipelineRef&>& it : shaders)
    {
        AssertThrow(it.first.IsValid());

        const DescriptorTableDeclaration& descriptor_table_decl = it.first->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("DDGIUniforms"), m_uniform_buffer);
            descriptor_set->SetElement(NAME("ProbeRayData"), m_radiance_buffer);

            descriptor_set->SetElement(NAME("OutputIrradianceImage"), m_irradiance_image_view);
            descriptor_set->SetElement(NAME("OutputDepthImage"), m_depth_image_view);
        }

        DeferCreate(descriptor_table);

        it.second = g_render_backend->MakeComputePipeline(
            it.first,
            descriptor_table);

        DeferCreate(it.second);
    }
}

void DDGI::CreateUniformBuffer()
{
    m_uniform_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms));

    const Vec2u grid_image_dimensions = m_grid_info.GetImageDimensions();
    const Vec3u num_probes_per_dimension = m_grid_info.NumProbesPerDimension();

    m_uniforms = DDGIUniforms {
        .aabb_max = Vec4f(m_grid_info.aabb.max, 1.0f),
        .aabb_min = Vec4f(m_grid_info.aabb.min, 1.0f),
        .probe_border = {
            m_grid_info.probe_border.x,
            m_grid_info.probe_border.y,
            m_grid_info.probe_border.z,
            0 },
        .probe_counts = { num_probes_per_dimension.x, num_probes_per_dimension.y, num_probes_per_dimension.z, 0 },
        .grid_dimensions = { grid_image_dimensions.x, grid_image_dimensions.y, 0, 0 },
        .image_dimensions = { m_irradiance_image->GetExtent().x, m_irradiance_image->GetExtent().y, m_depth_image->GetExtent().x, m_depth_image->GetExtent().y },
        .params = { ByteUtil::PackFloat(m_grid_info.probe_distance), m_grid_info.num_rays_per_probe, PROBE_SYSTEM_FLAGS_FIRST_RUN, 0 }
    };

    PUSH_RENDER_COMMAND(
        CreateDDGIUniformBuffer,
        m_uniform_buffer,
        m_uniforms);
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::SSBO, m_grid_info.GetImageDimensions().x * m_grid_info.GetImageDimensions().y * sizeof(ProbeRayData));

    PUSH_RENDER_COMMAND(
        CreateDDGIRadianceBuffer,
        m_radiance_buffer,
        m_grid_info);

    { // irradiance image
        const Vec3u extent {
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.x * probe_counts.y + 2,
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.z + 2,
            1
        };

        m_irradiance_image = g_render_backend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgi_irradiance_format,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_irradiance_image);
    }

    { // irradiance image view
        m_irradiance_image_view = g_render_backend->MakeImageView(m_irradiance_image);

        DeferCreate(m_irradiance_image_view);
    }

    { // depth image
        const Vec3u extent {
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.x * probe_counts.y + 2,
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.z + 2,
            1
        };

        m_depth_image = g_render_backend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgi_depth_format,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_depth_image);
    }

    { // depth image view
        m_depth_image_view = g_render_backend->MakeImageView(m_depth_image);

        DeferCreate(m_depth_image_view);
    }
}

void DDGI::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags)
    {
        return;
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = m_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE)
        {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);
        }

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS)
        {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());
        }

        descriptor_set->Update();

        m_updates[frame_index] &= ~PROBE_SYSTEM_UPDATES_TLAS;
    }
}

void DDGI::UpdateUniforms(FrameBase* frame)
{
    // FIXME: Lights are now stored per-view.
    // We don't have a View for DDGI since it is for the entire RenderWorld it is indirectly attached to.
    // We'll need to find a way to get the lights for the current view.
    // Ideas:
    // a) create a View for the DDGI and use that to get the lights. It will need to collect the lights on the Game thread so we'll need to add some kind of System to do that.
    // b) add a function to the RenderScene to get all the lights in the scene and use that to get the lights for the current view. This has a drawback that we will always have some RenderLight active when it could be inactive if it is not in any view.
    // OR: We can just use the lights in the current view and ignore the rest. This is a bit of a hack but it will work for now.
    HYP_NOT_IMPLEMENTED();

    m_uniforms.params[3] = 0;

    m_uniform_buffer->Copy(sizeof(DDGIUniforms), &m_uniforms);

    m_uniforms.params[2] &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(render_setup.IsValid());
    AssertThrow(render_setup.HasView());
    AssertThrow(render_setup.pass_data != nullptr);

    UpdateUniforms(frame);

    frame->GetCommandList().Add<InsertBarrier>(m_radiance_buffer, RS_UNORDERED_ACCESS);

    m_random_generator.Next();

    struct
    {
        Matrix4 matrix;
        uint32 time;
    } push_constants;

    push_constants.matrix = m_random_generator.matrix;
    push_constants.time = m_time++;

    m_pipeline->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<BindRaytracingPipeline>(m_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_pipeline->GetDescriptorTable(),
        m_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid ? render_setup.env_grid->GetRenderResource().GetBufferIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe ? render_setup.env_probe->GetRenderResource().GetBufferIndex() : 0) } } } },
        frame->GetFrameIndex());

    // bind per-view descriptor sets
    frame->GetCommandList().Add<BindDescriptorSet>(
        render_setup.pass_data->descriptor_sets[frame->GetFrameIndex()],
        m_pipeline);

    frame->GetCommandList().Add<TraceRays>(
        m_pipeline,
        Vec3u {
            m_grid_info.NumProbes(),
            m_grid_info.num_rays_per_probe,
            1u });

    frame->GetCommandList().Add<InsertBarrier>(
        m_radiance_buffer,
        RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes

    const Vec3u probe_counts = m_grid_info.NumProbesPerDimension();

    frame->GetCommandList().Add<InsertBarrier>(
        m_irradiance_image,
        RS_UNORDERED_ACCESS);

    frame->GetCommandList().Add<InsertBarrier>(
        m_depth_image,
        RS_UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(m_update_irradiance);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_update_irradiance->GetDescriptorTable(),
        m_update_irradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_update_irradiance,
        Vec3u {
            probe_counts.x * probe_counts.y,
            probe_counts.z,
            1u });

    frame->GetCommandList().Add<BindComputePipeline>(m_update_depth);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_update_depth->GetDescriptorTable(),
        m_update_depth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid ? render_setup.env_grid->GetRenderResource().GetBufferIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe ? render_setup.env_probe->GetRenderResource().GetBufferIndex() : 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        m_update_depth,
        Vec3u {
            probe_counts.x * probe_counts.y,
            probe_counts.z,
            1u });

#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    m_irradiance_image->InsertBarrier(
        frame->GetCommandList(),
        RS_UNORDERED_ACCESS
    );

    m_depth_image->InsertBarrier(
        frame->GetCommandList(),
        RS_UNORDERED_ACCESS
    );

    // now copy border texels
    m_copy_border_texels_irradiance->Bind(frame->GetCommandList());

    m_copy_border_texels_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_irradiance,
        {
            {
                NAME("Global"),
                {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe.Get(), 0) }
                }
            }
        }
    );

    m_copy_border_texels_irradiance->Dispatch(
        frame->GetCommandList(),
        Vec3u {
            (probe_counts.x * probe_counts.y * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.x)) + 7 / 8,
            (probe_counts.z * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.z)) + 7 / 8,
            1u
        }
    );
    
    m_copy_border_texels_depth->Bind(frame->GetCommandList());

    m_copy_border_texels_depth->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_depth,
        {
            {
                NAME("Global"),
                {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_camera) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe.Get(), 0) }
                }
            }
        }
    );
    
    m_copy_border_texels_depth->Dispatch(
        frame->GetCommandList(),
        Vec3u {
            (probe_counts.x * probe_counts.y * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.x)) + 15 / 16,
            (probe_counts.z * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.z)) + 15 / 16,
            1u
        }
    );

    m_irradiance_image->InsertBarrier(
        frame->GetCommandList(),
        RS_SHADER_RESOURCE
    );

    m_depth_image->InsertBarrier(
        frame->GetCommandList(),
        RS_SHADER_RESOURCE
    );
#endif
}

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, 1, sizeof(DDGIUniforms), false);
HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture, 1);

} // namespace hyperion