/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalBlending.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>

#include <core/threading/Threads.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RecreateTemporalBlendingFramebuffer) : renderer::RenderCommand
{
    TemporalBlending    &temporal_blending;
    Vec2u               new_size;

    RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)(TemporalBlending &temporal_blending, const Vec2u &new_size)
        : temporal_blending(temporal_blending),
          new_size(new_size)
    {
    }

    virtual ~RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)() override = default;

    virtual Result operator()() override
    {
        temporal_blending.Resize_Internal(new_size);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalBlending::TemporalBlending(
    const Vec2u &extent,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : TemporalBlending(
        extent,
        InternalFormat::RGBA8,
        technique,
        feedback,
        input_image_views
    )
{
}

TemporalBlending::TemporalBlending(
    const Vec2u &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FramebufferRef &input_framebuffer
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_framebuffer(input_framebuffer),
    m_blending_frame_counter(0),
    m_is_initialized(false)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_image_views(input_image_views),
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
    if (m_is_initialized) {
        return;
    }

    m_after_swapchain_recreated_delegate = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([this]()
    {
        if (!m_is_initialized) {
            return;
        }

        const ImageViewRef &velocity_texture = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView();

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
                ->SetElement(NAME("VelocityImage"), velocity_texture);
        }
    });

    if (m_input_framebuffer.IsValid()) {
        DeferCreate(m_input_framebuffer, g_engine->GetGPUDevice());
    }
    
    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();

    m_is_initialized = true;
}

void TemporalBlending::Resize(Vec2u new_size)
{
    PUSH_RENDER_COMMAND(RecreateTemporalBlendingFramebuffer, *this, new_size);
}

void TemporalBlending::Resize_Internal(Vec2u new_size)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_extent == new_size) {
        return;
    }

    m_extent = new_size;

    if (!m_is_initialized) {
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

    switch (m_image_format) {
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
        AssertThrowMsg(false, "Unsupported format for temporal blending: %u\n", uint(m_image_format));

        break;
    }

    static const String feedback_strings[] = { "LOW", "MEDIUM", "HIGH" };

    shader_properties.Set("TEMPORAL_BLEND_TECHNIQUE_" + String::ToString(uint(m_technique)));
    shader_properties.Set("FEEDBACK_" + feedback_strings[MathUtil::Min(uint(m_feedback), ArraySize(feedback_strings) - 1)]);

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
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_result_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_result_texture);

    m_history_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        m_image_format,
        Vec3u(m_extent, 1),
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_history_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_history_texture);
}

void TemporalBlending::CreateDescriptorSets()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    m_descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_input_framebuffer.IsValid()) {
            AssertThrowMsg(m_input_framebuffer->GetAttachmentMap().Size() != 0, "No attachment refs on input framebuffer!");
        }

        const ImageViewRef input_image_view = m_input_framebuffer.IsValid()
            ? m_input_framebuffer->GetAttachment(0)->GetImageView()
            : m_input_image_views[frame_index];

        AssertThrow(input_image_view != nullptr);

        // input image
        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("InImage"), input_image_view);

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("PrevImage"),  (*textures[(frame_index + 1) % 2])->GetImageView());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("VelocityImage"), g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("SamplerLinear"), g_engine->GetPlaceholderData()->GetSamplerLinear());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("SamplerNearest"), g_engine->GetPlaceholderData()->GetSamplerNearest());

        m_descriptor_table->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frame_index)
            ->SetElement(NAME("OutImage"), (*textures[frame_index % 2])->GetImageView());
    }

    DeferCreate(m_descriptor_table, g_engine->GetGPUDevice());
}

void TemporalBlending::CreateComputePipelines()
{
    AssertThrow(m_descriptor_table.IsValid());

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    AssertThrow(shader.IsValid());

    m_perform_blending = MakeRenderObject<ComputePipeline>(
        shader,
        m_descriptor_table
    );

    DeferCreate(m_perform_blending, g_engine->GetGPUDevice());
}

void TemporalBlending::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const TRenderResourcesHandle<CameraRenderResources> *active_camera = g_engine->GetRenderState().GetActiveCamera();
    const uint32 camera_index = active_camera != nullptr ? (*active_camera)->GetBufferIndex() : 0;
    AssertThrow(camera_index != ~0u);

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    Handle<Texture> &active_texture = *textures[frame->GetFrameIndex() % 2];

    active_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const Vec3u &extent = active_texture->GetExtent();
    const Vec3u depth_texture_dimensions = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
        .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImage()->GetExtent();

    struct alignas(128)
    {
        Vec2u   output_dimensions;
        Vec2u   depth_texture_dimensions;
        uint32  blending_frame_counter;
    } push_constants;

    push_constants.output_dimensions = Vec2u { extent.x, extent.y };
    push_constants.depth_texture_dimensions = Vec2u { depth_texture_dimensions.x, depth_texture_dimensions.y };

    push_constants.blending_frame_counter = m_blending_frame_counter;

    m_perform_blending->SetPushConstants(&push_constants, sizeof(push_constants));
    m_perform_blending->Bind(frame->GetCommandBuffer());

    m_descriptor_table->Bind(
        frame,
        m_perform_blending,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), HYP_SHADER_DATA_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { NAME("CamerasBuffer"), HYP_SHADER_DATA_OFFSET(Camera, camera_index) }
                }
            }
        }
    );

    m_perform_blending->Dispatch(
        frame->GetCommandBuffer(),
        Vec3u {
            (extent.x + 7) / 8,
            (extent.y + 7) / 8,
            1
        }
    );

    // set it to be able to be used as texture2D for next pass, or outside of this
    active_texture->GetImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

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
