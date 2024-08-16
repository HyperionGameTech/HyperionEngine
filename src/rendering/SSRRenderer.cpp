/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Threads.hpp>

#include <rendering/SSRRenderer.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;
using renderer::GPUBufferType;

static constexpr bool use_temporal_blending = true;

static constexpr InternalFormat ssr_format = InternalFormat::RGBA16F;

struct alignas(16) SSRParams
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
    Extent2D        extent;
    GPUBufferRef    uniform_buffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        Extent2D extent,
        GPUBufferRef uniform_buffers
    ) : extent(extent),
        uniform_buffer(std::move(uniform_buffers))
    {
        AssertThrow(extent.Size() != 0);
        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual Result operator()() override
    {
        const SSRParams ssr_params {
            .dimensions             = { extent.width, extent.height, 0, 0 },
            .ray_step               = 0.1f,
            .num_iterations         = 128.0f,
            .max_ray_distance       = 150.0f,
            .distance_bias          = 0.1f,
            .offset                 = 0.001f,
            .eye_fade_start         = 0.90f,
            .eye_fade_end           = 0.96f,
            .screen_edge_fade_start = 0.7f,
            .screen_edge_fade_end   = 0.98f
        };
            
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(ssr_params)
        ));

        uniform_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(ssr_params),
            &ssr_params
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

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSRResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

SSRRenderer::SSRRenderer(const Extent2D &extent, SSRRendererOptions options)
    : m_extent(extent),
      m_options(options),
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
        Extent3D(m_extent),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[0]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[0]);

    m_image_outputs[1] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Extent3D(m_extent),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[1]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[1]);

    m_image_outputs[2] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Extent3D(m_extent),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[2]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[2]);

    m_image_outputs[3] = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        ssr_format,
        Extent3D(m_extent),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_image_outputs[3]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[3]);

    CreateUniformBuffers();

    if (use_temporal_blending) {
        m_temporal_blending.Reset(new TemporalBlending(
            m_extent,
            InternalFormat::RGBA8,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::MEDIUM,
            FixedArray<ImageViewRef, 2> {
                m_image_outputs[1]->GetImageView(),
                m_image_outputs[1]->GetImageView()
            }
        ));

        m_temporal_blending->Create();
    }

    CreateComputePipelines();
}

void SSRRenderer::Destroy()
{
    m_is_rendered = false;

    SafeRelease(std::move(m_write_uvs));
    SafeRelease(std::move(m_sample));

    if (m_temporal_blending) {
        m_temporal_blending.Reset();
    }
    
    SafeRelease(std::move(m_uniform_buffer));

    PUSH_RENDER_COMMAND(RemoveSSRDescriptors);
}

ShaderProperties SSRRenderer::GetShaderProperties() const
{
    ShaderProperties shader_properties;
    shader_properties.Set("CONE_TRACING", bool(m_options & SSR_RENDERER_OPTIONS_CONE_TRACING));
    shader_properties.Set("ROUGHNESS_SCATTERING", bool(m_options & SSR_RENDERER_OPTIONS_ROUGHNESS_SCATTERING));

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
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateSSRUniformBuffer,
        m_extent,
        m_uniform_buffer
    );
}

void SSRRenderer::CreateComputePipelines()
{
    const ShaderProperties shader_properties = GetShaderProperties();

    // Write UVs pass

    ShaderRef write_uvs_shader = g_shader_manager->GetOrCreate(NAME("SSRWriteUVs"), shader_properties);
    AssertThrow(write_uvs_shader.IsValid());

    const renderer::DescriptorTableDeclaration write_uvs_shader_descriptor_table_decl = write_uvs_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef write_uvs_shader_descriptor_table = MakeRenderObject<DescriptorTable>(write_uvs_shader_descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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

    ShaderRef sample_shader = g_shader_manager->GetOrCreate(NAME("SSRSample"), shader_properties);
    AssertThrow(sample_shader.IsValid());

    const renderer::DescriptorTableDeclaration sample_shader_descriptor_table_decl = sample_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef sample_shader_descriptor_table = MakeRenderObject<DescriptorTable>(sample_shader_descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = sample_shader_descriptor_table->GetDescriptorSet(NAME("SSRDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UVImage"), m_image_outputs[0]->GetImageView());
        descriptor_set->SetElement(NAME("SampleImage"), m_image_outputs[1]->GetImageView());
        descriptor_set->SetElement(NAME("SSRParams"), m_uniform_buffer);
    }

    DeferCreate(sample_shader_descriptor_table, g_engine->GetGPUDevice());

    m_sample = MakeRenderObject<ComputePipeline>(
        sample_shader,
        sample_shader_descriptor_table
    );

    DeferCreate(m_sample, g_engine->GetGPUDevice());

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

    const uint scene_index = g_engine->render_state.GetScene().id.ToIndex();
    const uint camera_index = g_engine->render_state.GetCamera().id.ToIndex();

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");

    // We will be dispatching half the number of pixels, due to checkerboarding
    const SizeType total_pixels_in_image = m_extent.Size();
    const SizeType total_pixels_in_image_div_2 = total_pixels_in_image / 2;

    const uint32 num_dispatch_calls = (uint32(total_pixels_in_image_div_2) + 255) / 256;

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
                    { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                    { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                    { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                    { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                }
            }
        }
    );

    m_write_uvs->Dispatch(command_buffer, Extent3D {
        num_dispatch_calls, 1, 1
    });

    // transition the UV image back into read state
    m_image_outputs[0]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_image_outputs[1]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_sample->Bind(command_buffer);

    m_sample->GetDescriptorTable()->Bind(
        frame,
        m_sample,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                    { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                    { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                    { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                }
            }
        }
    );

    m_sample->Dispatch(command_buffer, Extent3D {
        num_dispatch_calls, 1, 1
    });

    // transition sample image back into read state
    m_image_outputs[1]->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    if (use_temporal_blending && m_temporal_blending != nullptr) {
        m_temporal_blending->Render(frame);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

} // namespace hyperion
