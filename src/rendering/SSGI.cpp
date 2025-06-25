/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SSGI.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <scene/Texture.hpp>
#include <scene/EnvProbe.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool use_temporal_blending = true;

static constexpr TextureFormat ssgi_format = TF_RGBA8;

struct SSGIUniforms
{
    Vec4u dimensions;
    float ray_step;
    float num_iterations;
    float max_ray_distance;
    float distance_bias;
    float offset;
    float eye_fade_start;
    float eye_fade_end;
    float screen_edge_fade_start;
    float screen_edge_fade_end;

    uint32 num_bound_lights;
    alignas(16) uint32 light_indices[16];
};

#pragma region Render commands

struct RENDER_COMMAND(CreateSSGIUniformBuffers)
    : RenderCommand
{
    SSGIUniforms uniforms;
    FixedArray<GPUBufferRef, max_frames_in_flight> uniform_buffers;

    RENDER_COMMAND(CreateSSGIUniformBuffers)(
        const SSGIUniforms& uniforms,
        const FixedArray<GPUBufferRef, max_frames_in_flight>& uniform_buffers)
        : uniforms(uniforms),
          uniform_buffers(uniform_buffers)
    {
        AssertThrow(uniforms.dimensions.x * uniforms.dimensions.y != 0);
    }

    virtual ~RENDER_COMMAND(CreateSSGIUniformBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            AssertThrow(uniform_buffers[frame_index] != nullptr);

            HYPERION_BUBBLE_ERRORS(uniform_buffers[frame_index]->Create());

            uniform_buffers[frame_index]->Copy(sizeof(uniforms), &uniforms);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region SSGI

SSGI::SSGI(SSGIConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_is_rendered(false)
{
}

SSGI::~SSGI()
{
    if (m_temporal_blending)
    {
        m_temporal_blending.Reset();
    }

    SafeRelease(std::move(m_uniform_buffers));
    SafeRelease(std::move(m_compute_pipeline));
}

void SSGI::Create()
{
    m_result_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        ssgi_format,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_result_texture);

    m_result_texture->SetPersistentRenderResourceEnabled(true);

    CreateUniformBuffers();

    if (use_temporal_blending)
    {
        m_temporal_blending = MakeUnique<TemporalBlending>(
            m_config.extent,
            ssgi_format,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::HIGH,
            m_result_texture->GetRenderResource().GetImageView(),
            m_gbuffer);

        m_temporal_blending->Create();
    }

    CreateComputePipelines();
}

const Handle<Texture>& SSGI::GetFinalResultTexture() const
{
    return m_temporal_blending
        ? m_temporal_blending->GetResultTexture()
        : m_result_texture;
}

ShaderProperties SSGI::GetShaderProperties() const
{
    ShaderProperties shader_properties;

    switch (ssgi_format)
    {
    case TF_RGBA8:
        shader_properties.Set("OUTPUT_RGBA8");
        break;
    case TF_RGBA16F:
        shader_properties.Set("OUTPUT_RGBA16F");
        break;
    case TF_RGBA32F:
        shader_properties.Set("OUTPUT_RGBA32F");
        break;
    }

    return shader_properties;
}

void SSGI::CreateUniformBuffers()
{
    SSGIUniforms uniforms;
    FillUniformBufferData(nullptr, uniforms);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_uniform_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(uniforms));
    }

    PUSH_RENDER_COMMAND(CreateSSGIUniformBuffers, uniforms, m_uniform_buffers);
}

void SSGI::CreateComputePipelines()
{
    const ShaderProperties shader_properties = GetShaderProperties();

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("SSGI"), shader_properties);
    AssertThrow(shader.IsValid());

    const DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("SSGIDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("OutImage"), m_result_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(descriptor_table);

    m_compute_pipeline = g_rendering_api->MakeComputePipeline(
        shader,
        descriptor_table);

    DeferCreate(m_compute_pipeline);
}

void SSGI::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_NAMED_SCOPE("Screen Space Global Illumination");

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 frame_index = frame->GetFrameIndex();

    // Update uniform buffer data
    SSGIUniforms uniforms;
    FillUniformBufferData(render_setup.view, uniforms);
    m_uniform_buffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);

    const uint32 total_pixels_in_image = m_config.extent.Volume();
    const uint32 num_dispatch_calls = (total_pixels_in_image + 255) / 256;

    // put sample image in writeable state
    frame->GetCommandList().Add<InsertBarrier>(m_result_texture->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(m_compute_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_compute_pipeline->GetDescriptorTable(),
        m_compute_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe ? render_setup.env_probe->GetRenderResource().GetBufferIndex() : 0) } } } },
        frame_index);

    const uint32 view_descriptor_set_index = m_compute_pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.pass_data != nullptr);

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_setup.pass_data->descriptor_sets[frame->GetFrameIndex()],
            m_compute_pipeline,
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);
    }

    frame->GetCommandList().Add<DispatchCompute>(m_compute_pipeline, Vec3u { num_dispatch_calls, 1, 1 });

    // transition sample image back into read state
    frame->GetCommandList().Add<InsertBarrier>(m_result_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);

    if (use_temporal_blending && m_temporal_blending != nullptr)
    {
        m_temporal_blending->Render(frame, render_setup);
    }

    m_is_rendered = true;
}

void SSGI::FillUniformBufferData(RenderView* view, SSGIUniforms& out_uniforms) const
{
    out_uniforms = SSGIUniforms();
    out_uniforms.dimensions = Vec4u(m_config.extent, 0, 0);
    out_uniforms.ray_step = 3.0f;
    out_uniforms.num_iterations = 8;
    out_uniforms.max_ray_distance = 1000.0f;
    out_uniforms.distance_bias = 0.1f;
    out_uniforms.offset = 0.001f;
    out_uniforms.eye_fade_start = 0.98f;
    out_uniforms.eye_fade_end = 0.99f;
    out_uniforms.screen_edge_fade_start = 0.98f;
    out_uniforms.screen_edge_fade_end = 0.99f;

    uint32 num_bound_lights = 0;

    // Can only fill the lights if we have a view ready
    if (view)
    {
        RenderProxyList& rpl = RendererAPI_GetConsumerProxyList(view->GetView());

        const uint32 max_bound_lights = ArraySize(out_uniforms.light_indices);

        for (uint32 light_type = 0; light_type < uint32(LT_MAX); light_type++)
        {
            if (num_bound_lights >= max_bound_lights)
            {
                break;
            }

            for (const auto& it : rpl.GetLights(LightType(light_type)))
            {
                if (num_bound_lights >= max_bound_lights)
                {
                    break;
                }

                out_uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
            }
        }
    }

    out_uniforms.num_bound_lights = num_bound_lights;
}

#pragma endregion SSGI

} // namespace hyperion
