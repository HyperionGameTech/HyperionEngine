/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SSRRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <scene/Texture.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::GPUBufferType;

static constexpr bool use_temporal_blending = true;

static constexpr InternalFormat ssr_format = InternalFormat::RGBA8;

struct SSRUniforms
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
    SSRUniforms     uniforms;
    GPUBufferRef    uniform_buffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        const SSRUniforms &uniforms,
        const GPUBufferRef &uniform_buffer
    ) : uniforms(uniforms),
        uniform_buffer(uniform_buffer)
    {
        AssertThrow(uniforms.dimensions.x * uniforms.dimensions.y != 0);

        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        DebugLog(LogType::Debug, "Sizeof(SSRUniforms) = %u\n", sizeof(SSRUniforms));
        
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(uniforms)
        ));
        
        DebugLog(LogType::Debug, "Size of buffer = %u\n",uniform_buffer->Size());
        uniform_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(uniforms),
            &uniforms
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSSRDescriptors) : renderer::RenderCommand
{
    ImageViewRef    image_view;

    RENDER_COMMAND(CreateSSRDescriptors)(const ImageViewRef &image_view)
        : image_view(image_view)
    {
    }

    virtual ~RENDER_COMMAND(CreateSSRDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSRResultTexture"), image_view);
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

Vec2u SSRRendererConfig::GetGBufferResolution()
{
    return g_engine->GetDeferredRenderer()->GetGBuffer()->GetResolution();
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
    m_uvs_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA16F,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_uvs_texture->SetIsRWTexture(true);
    InitObject(m_uvs_texture);

    m_uvs_texture->SetPersistentRenderResourceEnabled(true);

    m_sampled_result_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_sampled_result_texture->SetIsRWTexture(true);
    InitObject(m_sampled_result_texture);

    m_sampled_result_texture->SetPersistentRenderResourceEnabled(true);

    CreateUniformBuffers();

    if (use_temporal_blending) {
        m_temporal_blending = MakeUnique<TemporalBlending>(
            m_config.extent,
            InternalFormat::RGBA8,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::HIGH,
            m_sampled_result_texture->GetRenderResource().GetImageView()
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

const Handle<Texture> &SSRRenderer::GetFinalResultTexture() const
{
    return m_temporal_blending
        ? m_temporal_blending->GetResultTexture()
        : m_sampled_result_texture;
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
    SSRUniforms uniforms { };
    uniforms.dimensions = Vec4u(m_config.extent, 0, 0);
    uniforms.ray_step = m_config.ray_step;
    uniforms.num_iterations = m_config.num_iterations;
    uniforms.max_ray_distance = 150.0f;
    uniforms.distance_bias = 0.1f;
    uniforms.offset = 0.001f;
    uniforms.eye_fade_start = m_config.eye_fade.x;
    uniforms.eye_fade_end = m_config.eye_fade.y;
    uniforms.screen_edge_fade_start = m_config.screen_edge_fade.x;
    uniforms.screen_edge_fade_end = m_config.screen_edge_fade.y;

    m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
    
    PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, uniforms, m_uniform_buffer);
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

        descriptor_set->SetElement(NAME("UVImage"), m_uvs_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);
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

        descriptor_set->SetElement(NAME("UVImage"), m_uvs_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("SampleImage"), m_sampled_result_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);
    }

    DeferCreate(sample_gbuffer_shader_descriptor_table, g_engine->GetGPUDevice());

    m_sample_gbuffer = MakeRenderObject<ComputePipeline>(
        sample_gbuffer_shader,
        sample_gbuffer_shader_descriptor_table
    );

    DeferCreate(m_sample_gbuffer, g_engine->GetGPUDevice());

    PUSH_RENDER_COMMAND(
        CreateSSRDescriptors,
        GetFinalResultTexture()->GetRenderResource().GetImageView()
    );
}

void SSRRenderer::Render(Frame *frame)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");

    const uint32 total_pixels_in_image = m_config.extent.Volume();

    const uint32 num_dispatch_calls = (total_pixels_in_image + 255) / 256;

    // PASS 1 -- write UVs

    m_uvs_texture->GetRenderResource().GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_write_uvs->Bind(command_buffer);

    m_write_uvs->GetDescriptorTable()->Bind(
        frame,
        m_write_uvs,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) }
                }
            }
        }
    );

    m_write_uvs->Dispatch(command_buffer, Vec3u { num_dispatch_calls, 1, 1 });

    // transition the UV image back into read state
    m_uvs_texture->GetRenderResource().GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_sampled_result_texture->GetRenderResource().GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_sample_gbuffer->Bind(command_buffer);

    m_sample_gbuffer->GetDescriptorTable()->Bind(
        frame,
        m_sample_gbuffer,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) }
                }
            }
        }
    );

    m_sample_gbuffer->Dispatch(command_buffer, Vec3u { num_dispatch_calls, 1, 1 });

    // transition sample image back into read state
    m_sampled_result_texture->GetRenderResource().GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    if (use_temporal_blending && m_temporal_blending != nullptr) {
        m_temporal_blending->Render(frame);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

#pragma endregion SSRRenderer

} // namespace hyperion
