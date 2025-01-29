/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Camera.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet) : renderer::RenderCommand
{
    Handle<Texture> result_texture;

    RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet)(
        Handle<Texture> result_texture
    ) : result_texture(std::move(result_texture))
    {
    }

    virtual RendererResult operator()()
    {
        const ImageViewRef &result_texture_view = result_texture.IsValid()
            ? result_texture->GetImageView()
            : g_engine->GetPlaceholderData()->GetImageView2D1x1R8();

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("TAAResultTexture"), result_texture_view);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalAA::TemporalAA(const Vec2u &extent)
    : m_extent(extent),
      m_is_initialized(false)
{
}

TemporalAA::~TemporalAA()
{
    SafeRelease(std::move(m_compute_taa));

    g_safe_deleter->SafeRelease(std::move(m_result_texture));
    g_safe_deleter->SafeRelease(std::move(m_history_texture));

    if (m_is_initialized) {
        PUSH_RENDER_COMMAND(
            SetTemporalAAResultInGlobalDescriptorSet,
            Handle<Texture>::empty
        );
    }
}

void TemporalAA::Create()
{
    if (m_is_initialized) {
        return;
    }

    // Handle GBuffer size changed
    g_engine->GetDeferredRenderer()->GetGBuffer()->OnGBufferResolutionChanged.Bind([this](...)
    {
        HYP_SCOPE;
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        if (!m_is_initialized) {
            return;
        }

        SafeRelease(std::move(m_compute_taa));

        CreateComputePipelines();
    }).Detach();

    CreateImages();
    CreateComputePipelines();

    m_is_initialized = true;
}

void TemporalAA::CreateImages()
{
    m_result_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA8,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });
    
    m_result_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_result_texture);

    m_history_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA8,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_history_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_history_texture);

    PUSH_RENDER_COMMAND(
        SetTemporalAAResultInGlobalDescriptorSet,
        m_result_texture
    );
}

void TemporalAA::CreateComputePipelines()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("TemporalAA"));
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // create descriptor sets for depth pyramid generation.
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("TemporalAADescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InColorTexture"), g_engine->GetDeferredRenderer()->GetCombinedResult()->GetImageView());
        descriptor_set->SetElement(NAME("InPrevColorTexture"), (*textures[(frame_index + 1) % 2])->GetImageView());

        descriptor_set->SetElement(NAME("InVelocityTexture"), g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        descriptor_set->SetElement(NAME("InDepthTexture"), g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
    
        descriptor_set->SetElement(NAME("SamplerLinear"), g_engine->GetPlaceholderData()->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_engine->GetPlaceholderData()->GetSamplerNearest());

        descriptor_set->SetElement(NAME("OutColorImage"), (*textures[frame_index % 2])->GetImageView());
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_compute_taa = MakeRenderObject<ComputePipeline>(
        shader,
        descriptor_table
    );

    DeferCreate(
        m_compute_taa,
        g_engine->GetGPUDevice()
    );
}

void TemporalAA::Render(Frame *frame)
{
    HYP_NAMED_SCOPE("Temporal AA");

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();
    
    const CameraRenderResources *camera_render_resources = &g_engine->GetRenderState()->GetActiveCamera();

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    Handle<Texture> &active_texture = *textures[frame->GetFrameIndex() % 2];

    active_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    struct alignas(128) {
        Vec2u   dimensions;
        Vec2u   depth_texture_dimensions;
        Vec2f   camera_near_far;
    } push_constants;

    const Vec3u depth_texture_dimensions = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_OPAQUE)
        .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImage()->GetExtent();

    push_constants.dimensions = m_extent;
    push_constants.depth_texture_dimensions = Vec2u { depth_texture_dimensions.x, depth_texture_dimensions.y };
    push_constants.camera_near_far = Vec2f { camera_render_resources->GetBufferData().camera_near, camera_render_resources->GetBufferData().camera_far };

    m_compute_taa->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->Bind(command_buffer);

    m_compute_taa->GetDescriptorTable()->Bind(frame, m_compute_taa, { });

    m_compute_taa->Dispatch(
        command_buffer,
        Vec3u {
            (m_extent.x + 7) / 8,
            (m_extent.y + 7) / 8,
            1
        }
    );
    
    active_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion