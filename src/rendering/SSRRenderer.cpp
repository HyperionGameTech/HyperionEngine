/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Threads.hpp>

#include <rendering/SSRRenderer.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::GPUBufferType;

static constexpr bool use_temporal_blending = true;

static constexpr InternalFormat ssr_format = InternalFormat::RGBA16F;

struct SSRParams
{
    Vec4u   dimensions;
    float   ray_step,
            num_iterations,
            max_ray_distance,
            distance_bias,
            offset,
            eye_fade_start,
            eye_fade_end,
            screen_edge_fade_start,
            screen_edge_fade_end;
};

#pragma region Render commands

struct RENDER_COMMAND(CreateSSRUniformBuffer) : renderer::RenderCommand
{
    SSRParams       params;
    GPUBufferRef    uniform_buffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        const SSRParams &params,
        GPUBufferRef uniform_buffer
    ) : params(params),
        uniform_buffer(std::move(uniform_buffer))
    {
        AssertThrow(params.dimensions.x * params.dimensions.y != 0);

        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(params)
        ));

        uniform_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(params),
            &params
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSSRDescriptors) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateSSRDescriptors)(
        const FixedArray<ImageViewRef, max_frames_in_flight> &image_views
    ) : image_views(image_views)
    {
    }

    virtual ~RENDER_COMMAND(CreateSSRDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSRResultTexture"), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveSSRDescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(RemoveSSRDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(RemoveSSRDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSRResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region SSRRendererConfig

Vec2u SSRRendererConfig::GetSwapchainExtent()
{
    return g_engine->GetGPUInstance()->GetSwapchain()->extent;
}

#pragma endregion SSRRendererConfig

#pragma region SSRRenderer

SSRRenderer::SSRRenderer(SSRRendererConfig &&config)
    : m_config(std::move(config)),
      m_is_rendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
}

void SSRRenderer::Create()
{
    m_image_outputs[0] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[0]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[0]);

    m_image_outputs[1] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[1]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[1]);

    m_image_outputs[2] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[2]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[2]);

    m_image_outputs[3] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[3]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[3]);

    CreateUniformBuffers();

    if (use_temporal_blending) {
        m_temporal_blending = MakeUnique<TemporalBlending>(
            m_config.extent,
            InternalFormat::RGBA8,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::MEDIUM,
            FixedArray<ImageViewRef, 2> {
                m_image_outputs[1]->GetImageView(),
                m_image_outputs[1]->GetImageView()
            }
        );

        m_temporal_blending->Create();
    }

    CreateComputePipelines();
}

void SSRRenderer::Destroy()
{
    m_is_rendered = false;

    SafeRelease(std::move(m_write_uvs));
    SafeRelease(std::move(m_sample_gbuffer));

    if (m_temporal_blending) {
        m_temporal_blending.Reset();
    }
    
    SafeRelease(std::move(m_uniform_buffer));

    PUSH_RENDER_COMMAND(RemoveSSRDescriptors);
}

ShaderProperties SSRRenderer::GetShaderProperties() const
{
    ShaderProperties shader_properties;
    shader_properties.Set("CONE_TRACING", m_config.cone_tracing);
    shader_properties.Set("ROUGHNESS_SCATTERING", m_config.roughness_scattering);

    switch (ssr_format) {
    case InternalFormat::RGBA8:
        shader_properties.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_properties.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_properties.Set("OUTPUT_RGBA32F");
        break;
    }

    return shader_properties;
}

void SSRRenderer::CreateUniformBuffers()
{
    SSRParams params { };
    params.dimensions = Vec4u(m_config.extent, 0, 0);
    params.ray_step = m_config.ray_step;
    params.num_iterations = m_config.num_iterations;
    params.max_ray_distance = 1000.0f;
    params.distance_bias = 0.01f;
    params.offset = 0.001f;
    params.eye_fade_start = m_config.eye_fade.x;
    params.eye_fade_end = m_config.eye_fade.y;
    params.screen_edge_fade_start = m_config.screen_edge_fade.x;
    params.screen_edge_fade_end = m_config.screen_edge_fade.y;

    m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
    
    PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, params, m_uniform_buffer);
}

void SSRRenderer::CreateComputePipelines()
{
    const ShaderProperties shader_properties = GetShaderProperties();

    // Write UVs pass

    ShaderRef write_uvs_shader = g_shader_manager->GetOrCreate(NAME("SSRWriteUVs"), shader_properties);
    AssertThrow(write_uvs_shader.IsValid());

    const renderer::DescriptorTableDeclaration write_uvs_shader_descriptor_table_decl = write_uvs_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef write_uvs_shader_descriptor_table = MakeRenderObject<DescriptorTable>(write_uvs_shader_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = write_uvs_shader_descriptor_table->GetDescriptorSet(NAME("SSRDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UVImage"), m_image_outputs[0]->GetImageView());
        descriptor_set->SetElement(NAME("SSRParams"), m_uniform_buffer);
    }

    DeferCreate(write_uvs_shader_descriptor_table, g_engine->GetGPUDevice());

    m_write_uvs = MakeRenderObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(NAME("SSRWriteUVs"), shader_properties),
        write_uvs_shader_descriptor_table
    );

    DeferCreate(m_write_uvs, g_engine->GetGPUDevice());

    // Sample pass

    ShaderRef sample_gbuffer_shader = g_shader_manager->GetOrCreate(NAME("SSRSampleGBuffer"), shader_properties);
    AssertThrow(sample_gbuffer_shader.IsValid());

    const renderer::DescriptorTableDeclaration sample_gbuffer_shader_descriptor_table_decl = sample_gbuffer_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef sample_gbuffer_shader_descriptor_table = MakeRenderObject<DescriptorTable>(sample_gbuffer_shader_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = sample_gbuffer_shader_descriptor_table->GetDescriptorSet(NAME("SSRDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UVImage"), m_image_outputs[0]->GetImageView());
        descriptor_set->SetElement(NAME("SampleImage"), m_image_outputs[1]->GetImageView());
        descriptor_set->SetElement(NAME("SSRParams"), m_uniform_buffer);
    }

    DeferCreate(sample_gbuffer_shader_descriptor_table, g_engine->GetGPUDevice());

    m_sample_gbuffer = MakeRenderObject<ComputePipeline>(
        sample_gbuffer_shader,
        sample_gbuffer_shader_descriptor_table
    );

    DeferCreate(m_sample_gbuffer, g_engine->GetGPUDevice());

    PUSH_RENDER_COMMAND(
        CreateSSRDescriptors,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending ? m_temporal_blending->GetResultTexture()->GetImageView() : m_image_outputs[1]->GetImageView(),
            m_temporal_blending ? m_temporal_blending->GetResultTexture()->GetImageView() : m_image_outputs[1]->GetImageView()
        }
    );
}

void SSRRenderer::Render(Frame *frame)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    const SceneRenderResources *scene_render_resources = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResources *camera_render_resources = &g_engine->GetRenderState()->GetActiveCamera();

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");

    // We will be dispatching half the number of pixels, due to checkerboarding
    const uint32 total_pixels_in_image = m_config.extent.Volume();
    const uint32 total_pixels_in_image_div_2 = total_pixels_in_image / 2;

    const uint32 num_dispatch_calls = (total_pixels_in_image_div_2 + 255) / 256;

    // PASS 1 -- write UVs

    m_image_outputs[0]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_write_uvs->Bind(command_buffer);

    m_write_uvs->GetDescriptorTable()->Bind(
        frame,
        m_write_uvs,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) }
                }
            }
        }
    );

    m_write_uvs->Dispatch(command_buffer, Vec3u { num_dispatch_calls, 1, 1 });

    // transition the UV image back into read state
    m_image_outputs[0]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_image_outputs[1]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_sample_gbuffer->Bind(command_buffer);

    m_sample_gbuffer->GetDescriptorTable()->Bind(
        frame,
        m_sample_gbuffer,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resources) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resources) }
                }
            }
        }
    );

    m_sample_gbuffer->Dispatch(command_buffer, Vec3u { num_dispatch_calls, 1, 1 });

    // transition sample image back into read state
    m_image_outputs[1]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    if (use_temporal_blending && m_temporal_blending != nullptr) {
        m_temporal_blending->Render(frame);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

#pragma endregion SSRRenderer

} // namespace hyperion
