/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/DDGI.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

using renderer::ResourceState;

enum ProbeSystemUpdates : uint32
{
    PROBE_SYSTEM_UPDATES_NONE = 0x0,
    PROBE_SYSTEM_UPDATES_TLAS = 0x1
};

static constexpr InternalFormat ddgi_irradiance_format = InternalFormat::RGBA16F;
static constexpr InternalFormat ddgi_depth_format = InternalFormat::RG16F;

#pragma region Render commands

struct RENDER_COMMAND(SetDDGIDescriptors) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;
    ImageViewRef irradiance_image_view;
    ImageViewRef depth_image_view;

    RENDER_COMMAND(SetDDGIDescriptors)(
        const GPUBufferRef &uniform_buffer,
        const ImageViewRef &irradiance_image_view,
        const ImageViewRef &depth_image_view
    ) : uniform_buffer(uniform_buffer),
        irradiance_image_view(irradiance_image_view),
        depth_image_view(depth_image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DDGIUniforms"), uniform_buffer);

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DDGIIrradianceTexture"), irradiance_image_view);

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DDGIDepthTexture"), depth_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDDGIDescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(UnsetDDGIDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(UnsetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {   
        // remove result image from global descriptor set
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DDGIIrradianceTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DDGIDepthTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;
    DDGIUniforms uniforms;

    RENDER_COMMAND(CreateDDGIUniformBuffer)(const GPUBufferRef &uniform_buffer, const DDGIUniforms &uniforms)
        : uniform_buffer(uniform_buffer),
          uniforms(uniforms)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(DDGIUniforms)));
        uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(DDGIUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIRadianceBuffer) : renderer::RenderCommand
{
    GPUBufferRef radiance_buffer;
    DDGIInfo grid_info;

    RENDER_COMMAND(CreateDDGIRadianceBuffer)(const GPUBufferRef &radiance_buffer, const DDGIInfo &grid_info)
        : radiance_buffer(radiance_buffer),
          grid_info(grid_info)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIRadianceBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(radiance_buffer->Create(g_engine->GetGPUDevice(), grid_info.GetImageDimensions().x * grid_info.GetImageDimensions().y * sizeof(ProbeRayData)));
        radiance_buffer->Memset(g_engine->GetGPUDevice(), grid_info.GetImageDimensions().x * grid_info.GetImageDimensions().y * sizeof(ProbeRayData), 0x0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

DDGI::DDGI(DDGIInfo &&grid_info)
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
    AssertThrowMsg(
        m_tlas.IsValid(),
        "Invalid TLAS provided"
    );

    DebugLog(
        LogType::Info,
        "Creating %u DDGI probes\n",
        m_grid_info.NumProbes()
    );

    const Vec3u grid = m_grid_info.NumProbesPerDimension();
    m_probes.Resize(m_grid_info.NumProbes());

    for (uint32 x = 0; x < grid.x; x++) {
        for (uint32 y = 0; y < grid.y; y++) {
            for (uint32 z = 0; z < grid.z; z++) {
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
        m_depth_image_view
    );
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
    m_shader = g_shader_manager->GetOrCreate(NAME("RTProbe"));
    AssertThrow(m_shader.IsValid());

    const DescriptorTableDeclaration raytracing_pipeline_descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef raytracing_pipeline_descriptor_table = MakeRenderObject<DescriptorTable>(raytracing_pipeline_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = raytracing_pipeline_descriptor_table->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_tlas);
        
        descriptor_set->SetElement(NAME("LightsBuffer"), g_engine->GetRenderData()->lights->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_tlas->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(NAME("DDGIUniforms"), m_uniform_buffer);
        descriptor_set->SetElement(NAME("ProbeRayData"), m_radiance_buffer);
    }

    DeferCreate(raytracing_pipeline_descriptor_table, g_engine->GetGPUDevice());

    // Create raytracing pipeline

    m_pipeline = MakeRenderObject<RaytracingPipeline>(
        m_shader,
        raytracing_pipeline_descriptor_table
    );

    DeferCreate(m_pipeline, g_engine->GetGPUDevice());

    ShaderRef update_irradiance_shader = g_shader_manager->GetOrCreate(NAME("RTProbeUpdateIrradiance"));
    ShaderRef update_depth_shader = g_shader_manager->GetOrCreate(NAME("RTProbeUpdateDepth"));
    ShaderRef copy_border_texels_irradiance_shader = g_shader_manager->GetOrCreate(NAME("RTCopyBorderTexelsIrradiance"));
    ShaderRef copy_border_texels_depth_shader = g_shader_manager->GetOrCreate(NAME("RTCopyBorderTexelsDepth"));

    Pair<ShaderRef, ComputePipelineRef &> shaders[] = {
        { update_irradiance_shader, m_update_irradiance },
        { update_depth_shader, m_update_depth },
        { copy_border_texels_irradiance_shader, m_copy_border_texels_irradiance },
        { copy_border_texels_depth_shader, m_copy_border_texels_depth }
    };

    for (const Pair<ShaderRef, ComputePipelineRef &> &it : shaders) {
        AssertThrow(it.first.IsValid());
        
        const DescriptorTableDeclaration descriptor_table_decl = it.first->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("DDGIUniforms"), m_uniform_buffer);
            descriptor_set->SetElement(NAME("ProbeRayData"), m_radiance_buffer);

            descriptor_set->SetElement(NAME("OutputIrradianceImage"), m_irradiance_image_view);
            descriptor_set->SetElement(NAME("OutputDepthImage"), m_depth_image_view);
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        it.second = MakeRenderObject<ComputePipeline>(
            it.first,
            descriptor_table
        );

        DeferCreate(it.second, g_engine->GetGPUDevice());
    }
}

void DDGI::CreateUniformBuffer()
{
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(UniformBuffer());

    const Vec2u grid_image_dimensions = m_grid_info.GetImageDimensions();
    const Vec3u num_probes_per_dimension = m_grid_info.NumProbesPerDimension();

    m_uniforms = DDGIUniforms {
        .aabb_max = Vec4f(m_grid_info.aabb.max, 1.0f),
        .aabb_min = Vec4f(m_grid_info.aabb.min, 1.0f),
        .probe_border = {
            m_grid_info.probe_border.x,
            m_grid_info.probe_border.y,
            m_grid_info.probe_border.z,
            0
        },
        .probe_counts = {
            num_probes_per_dimension.x,
            num_probes_per_dimension.y,
            num_probes_per_dimension.z,
            0
        },
        .grid_dimensions = {
            grid_image_dimensions.x,
            grid_image_dimensions.y,
            0, 0
        },
        .image_dimensions = {
            m_irradiance_image->GetExtent().x,
            m_irradiance_image->GetExtent().y,
            m_depth_image->GetExtent().x,
            m_depth_image->GetExtent().y
        },
        .params = {
            ByteUtil::PackFloat(m_grid_info.probe_distance),
            m_grid_info.num_rays_per_probe,
            PROBE_SYSTEM_FLAGS_FIRST_RUN,
            0
        }
    };

    PUSH_RENDER_COMMAND(
        CreateDDGIUniformBuffer,
        m_uniform_buffer,
        m_uniforms
    );
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer = MakeRenderObject<GPUBuffer>(StorageBuffer());

    PUSH_RENDER_COMMAND(
        CreateDDGIRadianceBuffer,
        m_radiance_buffer,
        m_grid_info
    );

    { // irradiance image
        const Vec3u extent {
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.x * probe_counts.y + 2,
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.z + 2,
            1
        };

        m_irradiance_image = MakeRenderObject<Image>(StorageImage(
            extent,
            ddgi_irradiance_format,
            ImageType::TEXTURE_TYPE_2D
        ));

        DeferCreate(m_irradiance_image, g_engine->GetGPUDevice());
    }

    { // irradiance image view
        m_irradiance_image_view = MakeRenderObject<ImageView>();

        DeferCreate(m_irradiance_image_view, g_engine->GetGPUDevice(), m_irradiance_image);
    }

    { // depth image
        const Vec3u extent {
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.x * probe_counts.y + 2,
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.z + 2,
            1
        };

        m_depth_image = MakeRenderObject<Image>(StorageImage(
            extent,
            ddgi_depth_format,
            ImageType::TEXTURE_TYPE_2D
        ));

        DeferCreate(m_depth_image, g_engine->GetGPUDevice());
    }

    { // depth image view
        m_depth_image_view = MakeRenderObject<ImageView>();

        DeferCreate(m_depth_image_view, g_engine->GetGPUDevice(), m_depth_image);
    }
}

void DDGI::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = m_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(NAME("TLAS"), m_tlas);
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_tlas->GetMeshDescriptionsBuffer());
        }

        descriptor_set->Update(g_engine->GetGPUDevice());

        m_updates[frame_index] &= ~PROBE_SYSTEM_UPDATES_TLAS;
    }
}

void DDGI::UpdateUniforms(Frame *frame)
{
    const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(m_uniforms.light_indices));
    uint32 num_bound_lights = 0;

    for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++) {
        if (num_bound_lights >= max_bound_lights) {
            break;
        }

        for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
            if (num_bound_lights >= max_bound_lights) {
                break;
            }

            m_uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
        }
    }

    m_uniforms.params[3] = num_bound_lights;

    m_uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(DDGIUniforms), &m_uniforms);

    m_uniforms.params[2] &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::RenderProbes(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    UpdateUniforms(frame);

    const SceneRenderResource *scene_render_resources = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resources = &g_engine->GetRenderState()->GetActiveCamera();

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);

    m_random_generator.Next();

    struct alignas(128)
    {
        Matrix4 matrix;
        uint32  time;
    } push_constants;

    push_constants.matrix = m_random_generator.matrix;
    push_constants.time = m_time++;

    m_pipeline->SetPushConstants(&push_constants, sizeof(push_constants));
    m_pipeline->Bind(frame->GetCommandBuffer());
    
    m_pipeline->GetDescriptorTable()->Bind(
        frame,
        m_pipeline,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Vec3u {
            m_grid_info.NumProbes(),
            m_grid_info.num_rays_per_probe,
            1u
        }
    );

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);
}

void DDGI::ComputeIrradiance(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    const SceneRenderResource *scene_render_resources = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resources = &g_engine->GetRenderState()->GetActiveCamera();

    const Vec3u probe_counts = m_grid_info.NumProbesPerDimension();

    m_irradiance_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );
    
    m_update_irradiance->Bind(frame->GetCommandBuffer());

    m_update_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_update_irradiance,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_update_irradiance->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            probe_counts.x * probe_counts.y,
            probe_counts.z,
            1u
        }
    );

    m_update_depth->Bind(frame->GetCommandBuffer());

    m_update_depth->GetDescriptorTable()->Bind(
        frame,
        m_update_depth,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_update_depth->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            probe_counts.x * probe_counts.y,
            probe_counts.z,
            1u
        }
    );
    
#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    m_irradiance_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    // now copy border texels
    m_copy_border_texels_irradiance->Bind(frame->GetCommandBuffer());

    m_copy_border_texels_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_irradiance,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(g_engine->GetRenderState()->GetScene().id.ToIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_index) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_copy_border_texels_irradiance->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            (probe_counts.x * probe_counts.y * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.x)) + 7 / 8,
            (probe_counts.z * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.z)) + 7 / 8,
            1u
        }
    );
    
    m_copy_border_texels_depth->Bind(frame->GetCommandBuffer());

    m_copy_border_texels_depth->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_depth,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(g_engine->GetRenderState()->GetScene().id.ToIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_index) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );
    
    m_copy_border_texels_depth->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            (probe_counts.x * probe_counts.y * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.x)) + 15 / 16,
            (probe_counts.z * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.z)) + 15 / 16,
            1u
        }
    );

    m_irradiance_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );

    m_depth_image->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );
#endif
}

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, 1, sizeof(DDGIUniforms), false);
HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture, 1);

} // namespace renderer

} // namespace hyperion