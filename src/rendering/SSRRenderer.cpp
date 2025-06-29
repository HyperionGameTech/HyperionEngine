/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SSRRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <scene/Texture.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool use_temporal_blending = true;

static constexpr TextureFormat ssr_format = TF_RGBA16F;

struct SSRUniforms
{
    Vec4u dimensions;
    float ray_step,
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

struct RENDER_COMMAND(CreateSSRUniformBuffer)
    : RenderCommand
{
    SSRUniforms uniforms;
    GpuBufferRef uniform_buffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        const SSRUniforms& uniforms,
        const GpuBufferRef& uniform_buffer)
        : uniforms(uniforms),
          uniform_buffer(uniform_buffer)
    {
        AssertThrow(uniforms.dimensions.x * uniforms.dimensions.y != 0);

        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create());

        uniform_buffer->Copy(sizeof(uniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region SSRRenderer

SSRRenderer::SSRRenderer(
    SSRRendererConfig&& config,
    GBuffer* gbuffer,
    const ImageViewRef& mip_chain_image_view,
    const ImageViewRef& deferred_result_image_view)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_mip_chain_image_view(mip_chain_image_view),
      m_deferred_result_image_view(deferred_result_image_view),
      m_is_rendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
    SafeRelease(std::move(m_write_uvs));
    SafeRelease(std::move(m_sample_gbuffer));

    if (m_temporal_blending)
    {
        m_temporal_blending.Reset();
    }

    SafeRelease(std::move(m_uniform_buffer));
}

void SSRRenderer::Create()
{
    m_uvs_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_uvs_texture);

    m_uvs_texture->SetPersistentRenderResourceEnabled(true);

    m_sampled_result_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        ssr_format,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_sampled_result_texture);

    m_sampled_result_texture->SetPersistentRenderResourceEnabled(true);

    CreateUniformBuffers();

    if (use_temporal_blending)
    {
        m_temporal_blending = MakeUnique<TemporalBlending>(
            m_config.extent,
            TF_RGBA16F,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::HIGH,
            m_sampled_result_texture->GetRenderResource().GetImageView(),
            m_gbuffer);

        m_temporal_blending->Create();
    }

    CreateComputePipelines();

    // m_on_gbuffer_resolution_changed = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u new_size)
    // {
    //     SafeRelease(std::move(m_write_uvs));
    //     SafeRelease(std::move(m_sample_gbuffer));

    //     CreateComputePipelines();
    // });
}

const Handle<Texture>& SSRRenderer::GetFinalResultTexture() const
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

    switch (ssr_format)
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

void SSRRenderer::CreateUniformBuffers()
{
    SSRUniforms uniforms {};
    uniforms.dimensions = Vec4u(m_config.extent, 0, 0);
    uniforms.ray_step = m_config.ray_step;
    uniforms.num_iterations = m_config.num_iterations;
    uniforms.max_ray_distance = 1000.0f;
    uniforms.distance_bias = 0.1f;
    uniforms.offset = 0.001f;
    uniforms.eye_fade_start = m_config.eye_fade.x;
    uniforms.eye_fade_end = m_config.eye_fade.y;
    uniforms.screen_edge_fade_start = m_config.screen_edge_fade.x;
    uniforms.screen_edge_fade_end = m_config.screen_edge_fade.y;

    m_uniform_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));

    PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, uniforms, m_uniform_buffer);
}

void SSRRenderer::CreateComputePipelines()
{
    const ShaderProperties shader_properties = GetShaderProperties();

    // Write UVs pass

    ShaderRef write_uvs_shader = g_shader_manager->GetOrCreate(NAME("SSRWriteUVs"), shader_properties);
    AssertThrow(write_uvs_shader.IsValid());

    const DescriptorTableDeclaration& write_uvs_shader_descriptor_table_decl = write_uvs_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef write_uvs_shader_descriptor_table = g_render_backend->MakeDescriptorTable(&write_uvs_shader_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = write_uvs_shader_descriptor_table->GetDescriptorSet(NAME("SSRDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UVImage"), m_uvs_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);

        descriptor_set->SetElement(NAME("GBufferNormalsTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_NORMALS)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferMaterialTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_MATERIAL)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferVelocityTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_VELOCITY)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferDepthTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_DEPTH)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferMipChain"), m_mip_chain_image_view ? m_mip_chain_image_view : g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        descriptor_set->SetElement(NAME("DeferredResult"), m_deferred_result_image_view ? m_deferred_result_image_view : g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
    }

    DeferCreate(write_uvs_shader_descriptor_table);

    m_write_uvs = g_render_backend->MakeComputePipeline(
        g_shader_manager->GetOrCreate(NAME("SSRWriteUVs"), shader_properties),
        write_uvs_shader_descriptor_table);

    DeferCreate(m_write_uvs);

    // Sample pass

    ShaderRef sample_gbuffer_shader = g_shader_manager->GetOrCreate(NAME("SSRSampleGBuffer"), shader_properties);
    AssertThrow(sample_gbuffer_shader.IsValid());

    const DescriptorTableDeclaration& sample_gbuffer_shader_descriptor_table_decl = sample_gbuffer_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef sample_gbuffer_shader_descriptor_table = g_render_backend->MakeDescriptorTable(&sample_gbuffer_shader_descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = sample_gbuffer_shader_descriptor_table->GetDescriptorSet(NAME("SSRDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UVImage"), m_uvs_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("SampleImage"), m_sampled_result_texture->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);

        descriptor_set->SetElement(NAME("GBufferNormalsTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_NORMALS)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferMaterialTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_MATERIAL)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferVelocityTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_VELOCITY)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferDepthTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GBufferResourceName::GBUFFER_RESOURCE_DEPTH)->GetImageView());
        descriptor_set->SetElement(NAME("GBufferMipChain"), m_mip_chain_image_view ? m_mip_chain_image_view : g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        descriptor_set->SetElement(NAME("DeferredResult"), m_deferred_result_image_view ? m_deferred_result_image_view : g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
    }

    DeferCreate(sample_gbuffer_shader_descriptor_table);

    m_sample_gbuffer = g_render_backend->MakeComputePipeline(
        sample_gbuffer_shader,
        sample_gbuffer_shader_descriptor_table);

    DeferCreate(m_sample_gbuffer);
}

void SSRRenderer::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    const uint32 total_pixels_in_image = m_config.extent.Volume();

    const uint32 num_dispatch_calls = (total_pixels_in_image + 255) / 256;

    { // PASS 1 -- write UVs

        frame->GetCommandList().Add<InsertBarrier>(m_uvs_texture->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

        frame->GetCommandList().Add<BindComputePipeline>(m_write_uvs);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_write_uvs->GetDescriptorTable(),
            m_write_uvs,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame_index);

        const uint32 view_descriptor_set_index = m_write_uvs->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (view_descriptor_set_index != ~0u)
        {
            AssertThrow(render_setup.pass_data != nullptr);

            frame->GetCommandList().Add<BindDescriptorSet>(
                render_setup.pass_data->descriptor_sets[frame->GetFrameIndex()],
                m_write_uvs,
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);
        }

        frame->GetCommandList().Add<DispatchCompute>(m_write_uvs, Vec3u { num_dispatch_calls, 1, 1 });

        // transition the UV image back into read state
        frame->GetCommandList().Add<InsertBarrier>(m_uvs_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
    }

    { // PASS 2 - sample textures
        // put sample image in writeable state
        frame->GetCommandList().Add<InsertBarrier>(m_sampled_result_texture->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

        frame->GetCommandList().Add<BindComputePipeline>(m_sample_gbuffer);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_sample_gbuffer->GetDescriptorTable(),
            m_sample_gbuffer,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame_index);

        const uint32 view_descriptor_set_index = m_sample_gbuffer->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (view_descriptor_set_index != ~0u)
        {
            AssertThrow(render_setup.pass_data != nullptr);

            frame->GetCommandList().Add<BindDescriptorSet>(
                render_setup.pass_data->descriptor_sets[frame->GetFrameIndex()],
                m_sample_gbuffer,
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);
        }

        frame->GetCommandList().Add<DispatchCompute>(m_sample_gbuffer, Vec3u { num_dispatch_calls, 1, 1 });

        // transition sample image back into read state
        frame->GetCommandList().Add<InsertBarrier>(m_sampled_result_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
    }

    if (use_temporal_blending && m_temporal_blending != nullptr)
    {
        m_temporal_blending->Render(frame, render_setup);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

#pragma endregion SSRRenderer

} // namespace hyperion
