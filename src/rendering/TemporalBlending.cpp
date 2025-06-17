/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalBlending.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <scene/Texture.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)
    : renderer::RenderCommand
{
    TemporalBlending& temporal_blending;
    Vec2u new_size;

    RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)(TemporalBlending& temporal_blending, const Vec2u& new_size)
        : temporal_blending(temporal_blending),
          new_size(new_size)
    {
    }

    virtual ~RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)() override = default;

    virtual RendererResult operator()() override
    {
        temporal_blending.Resize_Internal(new_size);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const ImageViewRef& input_image_view,
    GBuffer* gbuffer)
    : TemporalBlending(
          extent,
          InternalFormat::RGBA8,
          technique,
          feedback,
          input_image_view,
          gbuffer)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FramebufferRef& input_framebuffer,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_image_format(image_format),
      m_technique(technique),
      m_feedback(feedback),
      m_input_framebuffer(input_framebuffer),
      m_gbuffer(gbuffer),
      m_blending_frame_counter(0),
      m_is_initialized(false)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const ImageViewRef& input_image_view,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_image_format(image_format),
      m_technique(technique),
      m_feedback(feedback),
      m_input_image_view(input_image_view),
      m_gbuffer(gbuffer),
      m_blending_frame_counter(0),
      m_is_initialized(false)
{
}

TemporalBlending::~TemporalBlending()
{
    SafeRelease(std::move(m_input_framebuffer));

    SafeRelease(std::move(m_perform_blending));
    SafeRelease(std::move(m_descriptor_table));

    g_safe_deleter->SafeRelease(std::move(m_result_texture));
    g_safe_deleter->SafeRelease(std::move(m_history_texture));
}

void TemporalBlending::Create()
{
    if (m_is_initialized)
    {
        return;
    }

    AssertThrow(m_gbuffer != nullptr);

    if (m_input_framebuffer.IsValid())
    {
        DeferCreate(m_input_framebuffer);
    }

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();

    m_on_gbuffer_resolution_changed = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u new_size)
        {
            Resize_Internal(new_size);
        });

    m_is_initialized = true;
}

void TemporalBlending::Resize(Vec2u new_size)
{
    PUSH_RENDER_COMMAND(RecreateTemporalBlendingFramebuffer, *this, new_size);
    HYP_SYNC_RENDER();
}

void TemporalBlending::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_extent == new_size)
    {
        return;
    }

    m_extent = new_size;

    if (!m_is_initialized)
    {
        return;
    }

    SafeRelease(std::move(m_perform_blending));
    SafeRelease(std::move(m_descriptor_table));

    g_safe_deleter->SafeRelease(std::move(m_result_texture));
    g_safe_deleter->SafeRelease(std::move(m_history_texture));

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();
}

ShaderProperties TemporalBlending::GetShaderProperties() const
{
    ShaderProperties shader_properties;

    switch (m_image_format)
    {
    case InternalFormat::RGBA8:
        shader_properties.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_properties.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_properties.Set("OUTPUT_RGBA32F");
        break;
    default:
        AssertThrowMsg(false, "Unsupported format for temporal blending: %u\n", uint32(m_image_format));

        break;
    }

    static const String feedback_strings[] = { "LOW", "MEDIUM", "HIGH" };

    shader_properties.Set("TEMPORAL_BLEND_TECHNIQUE_" + String::ToString(uint32(m_technique)));
    shader_properties.Set("FEEDBACK_" + feedback_strings[MathUtil::Min(uint32(m_feedback), ArraySize(feedback_strings) - 1)]);

    return shader_properties;
}

void TemporalBlending::CreateImageOutputs()
{
    m_result_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        m_image_format,
        Vec3u(m_extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_result_texture);

    m_result_texture->SetPersistentRenderResourceEnabled(true);

    m_history_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        m_image_format,
        Vec3u(m_extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_history_texture);

    m_history_texture->SetPersistentRenderResourceEnabled(true);
}

void TemporalBlending::CreateDescriptorSets()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    m_descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const ImageViewRef& input_image_view = m_input_framebuffer.IsValid()
            ? m_input_framebuffer->GetAttachment(0)->GetImageView()
            : m_input_image_view;

        AssertThrow(input_image_view != nullptr);

        // input image
        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("InImage"), input_image_view);

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("PrevImage"), (*textures[(frame_index + 1) % 2])->GetRenderResource().GetImageView());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("VelocityImage"), m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("OutImage"), (*textures[frame_index % 2])->GetRenderResource().GetImageView());
    }

    DeferCreate(m_descriptor_table);
}

void TemporalBlending::CreateComputePipelines()
{
    AssertThrow(m_descriptor_table.IsValid());

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    AssertThrow(shader.IsValid());

    m_perform_blending = g_rendering_api->MakeComputePipeline(shader, m_descriptor_table);
    DeferCreate(m_perform_blending);
}

void TemporalBlending::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const ImageRef& active_image = frame->GetFrameIndex() % 2 == 0
        ? m_result_texture->GetRenderResource().GetImage()
        : m_history_texture->GetRenderResource().GetImage();

    frame->GetCommandList().Add<InsertBarrier>(active_image, renderer::ResourceState::UNORDERED_ACCESS);

    const Vec3u& extent = active_image->GetExtent();
    const Vec3u depth_texture_dimensions = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE)
                                               .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)
                                               ->GetImage()
                                               ->GetExtent();

    struct
    {
        Vec2u output_dimensions;
        Vec2u depth_texture_dimensions;
        uint32 blending_frame_counter;
    } push_constants;

    push_constants.output_dimensions = Vec2u { extent.x, extent.y };
    push_constants.depth_texture_dimensions = Vec2u { depth_texture_dimensions.x, depth_texture_dimensions.y };

    push_constants.blending_frame_counter = m_blending_frame_counter;

    m_perform_blending->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<BindComputePipeline>(m_perform_blending);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_descriptor_table,
        m_perform_blending,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
        frame->GetFrameIndex());

    const uint32 view_descriptor_set_index = m_perform_blending->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (view_descriptor_set_index != ~0u)
    {
        frame->GetCommandList().Add<BindDescriptorSet>(
            render_setup.view->GetDescriptorSets()[frame->GetFrameIndex()],
            m_perform_blending,
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);
    }

    frame->GetCommandList().Add<DispatchCompute>(
        m_perform_blending,
        Vec3u {
            (extent.x + 7) / 8,
            (extent.y + 7) / 8,
            1 });

    // set it to be able to be used as texture2D for next pass, or outside of this
    frame->GetCommandList().Add<InsertBarrier>(active_image, renderer::ResourceState::SHADER_RESOURCE);

    m_blending_frame_counter = m_technique == TemporalBlendTechnique::TECHNIQUE_4
        ? m_blending_frame_counter + 1
        : 0;
}

void TemporalBlending::ResetProgressiveBlending()
{
    // roll over to 0 on next increment to add an extra frame
    m_blending_frame_counter = MathUtil::MaxSafeValue(m_blending_frame_counter);
}

} // namespace hyperion
