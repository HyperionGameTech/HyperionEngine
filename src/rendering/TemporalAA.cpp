/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <scene/Texture.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

TemporalAA::TemporalAA(const Vec2u& extent, const ImageViewRef& input_image_view, GBuffer* gbuffer)
    : m_extent(extent),
      m_input_image_view(input_image_view),
      m_gbuffer(gbuffer),
      m_is_initialized(false)
{
}

TemporalAA::~TemporalAA()
{
    SafeRelease(std::move(m_input_image_view));
    SafeRelease(std::move(m_compute_taa));
}

void TemporalAA::Create()
{
    if (m_is_initialized)
    {
        return;
    }

    AssertThrow(m_gbuffer != nullptr);

    CreateImages();
    CreateComputePipelines();

    m_on_gbuffer_resolution_changed = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u new_size)
        {
            SafeRelease(std::move(m_compute_taa));

            CreateComputePipelines();
        });

    m_is_initialized = true;
}

void TemporalAA::CreateImages()
{
    m_result_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_result_texture);

    m_result_texture->SetPersistentRenderResourceEnabled(true);

    m_history_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

    InitObject(m_history_texture);

    m_history_texture->SetPersistentRenderResourceEnabled(true);
}

void TemporalAA::CreateComputePipelines()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalAA"));
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        // create descriptor sets for depth pyramid generation.
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("TemporalAADescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InColorTexture"), m_input_image_view);
        descriptor_set->SetElement(NAME("InPrevColorTexture"), (*textures[(frame_index + 1) % 2])->GetRenderResource().GetImageView());

        descriptor_set->SetElement(NAME("InVelocityTexture"), m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        descriptor_set->SetElement(NAME("InDepthTexture"), m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        descriptor_set->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());

        descriptor_set->SetElement(NAME("OutColorImage"), (*textures[frame_index % 2])->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptor_table);

    m_compute_taa = g_rendering_api->MakeComputePipeline(
        shader,
        descriptor_table);

    DeferCreate(m_compute_taa);
}

void TemporalAA::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_NAMED_SCOPE("Temporal AA");

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 frame_index = frame->GetFrameIndex();

    const ImageRef& active_image = frame->GetFrameIndex() % 2 == 0
        ? m_result_texture->GetRenderResource().GetImage()
        : m_history_texture->GetRenderResource().GetImage();

    frame->GetCommandList().Add<InsertBarrier>(active_image, renderer::ResourceState::UNORDERED_ACCESS);

    struct
    {
        Vec2u dimensions;
        Vec2u depth_texture_dimensions;
        Vec2f camera_near_far;
    } push_constants;

    const Vec3u depth_texture_dimensions = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE)
                                               .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)
                                               ->GetImage()
                                               ->GetExtent();

    push_constants.dimensions = m_extent;
    push_constants.depth_texture_dimensions = Vec2u { depth_texture_dimensions.x, depth_texture_dimensions.y };
    push_constants.camera_near_far = Vec2f { render_setup.view->GetCamera()->GetBufferData().camera_near, render_setup.view->GetCamera()->GetBufferData().camera_far };

    m_compute_taa->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<BindComputePipeline>(m_compute_taa);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_compute_taa->GetDescriptorTable(),
        m_compute_taa,
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frame_index);

    frame->GetCommandList().Add<DispatchCompute>(m_compute_taa, Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->GetCommandList().Add<InsertBarrier>(active_image, renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion