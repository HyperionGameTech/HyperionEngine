/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <math/Vector2.hpp>
#include <math/MathUtil.hpp>
#include <math/Halton.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::ShaderVec2;
using renderer::ShaderMat4;

#pragma region Render commands

struct RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet) : renderer::RenderCommand
{
    Handle<Texture> result_texture;

    RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet)(
        Handle<Texture> result_texture
    ) : result_texture(std::move(result_texture))
    {
    }

    virtual Result operator()()
    {
        const ImageViewRef &result_texture_view = result_texture.IsValid()
            ? result_texture->GetImageView()
            : g_engine->GetPlaceholderData()->GetImageView2D1x1R8();

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(TAAResultTexture), result_texture_view);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalAA::TemporalAA(const Extent2D &extent)
    : m_extent(extent)
{
}

TemporalAA::~TemporalAA() = default;

void TemporalAA::Create()
{
    CreateImages();
    CreateComputePipelines();
}

void TemporalAA::Destroy()
{
    SafeRelease(std::move(m_compute_taa));

    PUSH_RENDER_COMMAND(
        SetTemporalAAResultInGlobalDescriptorSet,
        Handle<Texture>::empty
    );

    g_safe_deleter->SafeRelease(std::move(m_result_texture));
    g_safe_deleter->SafeRelease(std::move(m_history_texture));
}

void TemporalAA::CreateImages()
{
    m_result_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA16F,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_result_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_result_texture);

    m_history_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA16F,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_history_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_history_texture);

    PUSH_RENDER_COMMAND(
        SetTemporalAAResultInGlobalDescriptorSet,
        m_result_texture
    );
}

void TemporalAA::CreateComputePipelines()
{
    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(TemporalAA));

    const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    auto descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // create descriptor sets for depth pyramid generation.
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(TemporalAADescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(InColorTexture), g_engine->GetDeferredRenderer()->GetCombinedResult()->GetImageView());
        descriptor_set->SetElement(HYP_NAME(InPrevColorTexture), (*textures[(frame_index + 1) % 2])->GetImageView());

        descriptor_set->SetElement(HYP_NAME(InVelocityTexture), g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        descriptor_set->SetElement(HYP_NAME(InDepthTexture), g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
    
        descriptor_set->SetElement(HYP_NAME(SamplerLinear), g_engine->GetPlaceholderData()->GetSamplerLinear());
        descriptor_set->SetElement(HYP_NAME(SamplerNearest), g_engine->GetPlaceholderData()->GetSamplerNearest());

        descriptor_set->SetElement(HYP_NAME(OutColorImage), (*textures[frame_index % 2])->GetImageView());
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_compute_taa = MakeRenderObject<renderer::ComputePipeline>(
        shader->GetShaderProgram(),
        descriptor_table
    );

    DeferCreate(
        m_compute_taa,
        g_engine->GetGPUDevice()
    );
}

void TemporalAA::Render(Frame *frame)
{
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    const auto &scene = g_engine->GetRenderState().GetScene().scene;
    const auto &camera = g_engine->GetRenderState().GetCamera().camera;

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    Handle<Texture> &active_texture = *textures[frame->GetFrameIndex() % 2];

    active_texture->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    struct alignas(128) {
        ShaderVec2<uint32>  dimensions;
        ShaderVec2<uint32>  depth_texture_dimensions;
        ShaderVec2<float>   camera_near_far;
    } push_constants;

    push_constants.dimensions = m_extent;
    push_constants.depth_texture_dimensions = Extent2D(
        g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );
    push_constants.camera_near_far = Vector2(camera.clip_near, camera.clip_far);

    m_compute_taa->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->Bind(command_buffer);

    m_compute_taa->GetDescriptorTable()->Bind(frame, m_compute_taa, { });

    m_compute_taa->Dispatch(
        command_buffer,
        Extent3D {
            (m_extent.width + 7) / 8,
            (m_extent.height + 7) / 8,
            1
        }
    );
    
    active_texture->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion