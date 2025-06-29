/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PostFX.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

PostFXPass::PostFXPass(TextureFormat image_format, GBuffer* gbuffer)
    : PostFXPass(nullptr, POST_PROCESSING_STAGE_PRE_SHADING, ~0u, image_format, gbuffer)
{
}

PostFXPass::PostFXPass(const ShaderRef& shader, TextureFormat image_format, GBuffer* gbuffer)
    : PostFXPass(shader, POST_PROCESSING_STAGE_PRE_SHADING, ~0u, image_format, gbuffer)
{
}

PostFXPass::PostFXPass(const ShaderRef& shader, PostProcessingStage stage, uint32 effect_index, TextureFormat image_format, GBuffer* gbuffer)
    : FullScreenPass(shader, image_format, Vec2u::Zero(), gbuffer),
      m_stage(stage),
      m_effect_index(effect_index)
{
}

PostFXPass::~PostFXPass()
{
}

void PostFXPass::CreateDescriptors()
{
    Threads::AssertOnThread(g_render_thread);

    if (m_effect_index == ~0u)
    {
        HYP_LOG(Rendering, Warning, "Effect index not set, skipping descriptor creation");

        return;
    }

    if (!g_render_backend->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
    {
        HYP_LOG(Rendering, Warning, "Creating post processing pass on a device that does not support dynamic descriptor indexing");

        return;
    }

    // @TODO Reimplement

    // const Name descriptor_name = m_stage == POST_PROCESSING_STAGE_PRE_SHADING
    //     ? NAME("PostFXPreStack")
    //     : NAME("PostFXPostStack");

    // for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
    //     g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
    //         ->SetElement(descriptor_name, m_effect_index, m_framebuffer->GetAttachment(0)->GetImageView());
    // }
}

PostProcessingEffect::PostProcessingEffect(PostProcessingStage stage, uint32 effect_index, TextureFormat image_format, GBuffer* gbuffer)
    : m_pass(nullptr, stage, effect_index, image_format, gbuffer),
      m_is_enabled(true)
{
}

PostProcessingEffect::~PostProcessingEffect() = default;

void PostProcessingEffect::Init()
{
    m_shader = CreateShader();

    m_pass.SetShader(m_shader);
    m_pass.Create();
}

void PostProcessingEffect::RenderEffect(FrameBase* frame, const RenderSetup& render_setup, uint32 slot)
{
    struct
    {
        uint32 current_effect_index_stage; // 31bits for index, 1 bit for stage
    } push_constants;

    push_constants.current_effect_index_stage = (slot << 1) | uint32(m_pass.GetStage());

    m_pass.SetPushConstants(&push_constants, sizeof(push_constants));
    m_pass.Render(frame, render_setup);
}

PostProcessing::PostProcessing() = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create()
{
    Threads::AssertOnThread(g_render_thread);

    for (uint32 stage_index = 0; stage_index < 2; stage_index++)
    {
        for (auto& effect : m_effects[stage_index])
        {
            AssertThrow(effect.second != nullptr);

            effect.second->Init();

            effect.second->OnAdded();
        }
    }

    CreateUniformBuffer();

    PerformUpdates();
}

void PostProcessing::Destroy()
{
    Threads::AssertOnThread(g_render_thread);

    {
        std::lock_guard guard(m_effects_mutex);

        for (uint32 stage_index = 0; stage_index < 2; stage_index++)
        {
            m_effects_pending_addition[stage_index].Clear();
            m_effects_pending_removal[stage_index].Clear();
        }
    }

    for (uint32 stage_index = 0; stage_index < 2; stage_index++)
    {
        for (auto& it : m_effects[stage_index])
        {
            AssertThrow(it.second != nullptr);

            it.second->OnRemoved();
        }

        m_effects[stage_index].Clear();
    }

    SafeRelease(std::move(m_uniform_buffer));
}

void PostProcessing::PerformUpdates()
{
    Threads::AssertOnThread(g_render_thread);

    if (!m_effects_updated.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    std::lock_guard guard(m_effects_mutex);

    for (SizeType stage_index = 0; stage_index < 2; stage_index++)
    {
        for (auto& it : m_effects_pending_addition[stage_index])
        {
            const TypeId type_id = it.first;
            auto& effect = it.second;

            AssertThrow(effect != nullptr);

            effect->Init();

            effect->OnAdded();

            m_effects[stage_index].Set(type_id, std::move(effect));
        }

        m_effects_pending_addition[stage_index].Clear();

        for (const TypeId type_id : m_effects_pending_removal[stage_index])
        {
            const auto effects_it = m_effects[stage_index].Find(type_id);

            if (effects_it != m_effects[stage_index].End())
            {
                AssertThrow(effects_it->second != nullptr);

                effects_it->second->OnRemoved();

                m_effects[stage_index].Erase(effects_it);
            }
        }

        m_effects_pending_removal[stage_index].Clear();
    }

    m_effects_updated.Set(false, MemoryOrder::RELEASE);

    HYP_SYNC_RENDER();
}

PostProcessingUniforms PostProcessing::GetUniforms() const
{
    PostProcessingUniforms post_processing_uniforms {};

    for (uint32 stage_index = 0; stage_index < 2; stage_index++)
    {
        auto& effects = m_effects[stage_index];

        post_processing_uniforms.effect_counts[stage_index] = uint32(effects.Size());
        post_processing_uniforms.masks[stage_index] = 0u;
        post_processing_uniforms.last_enabled_indices[stage_index] = 0u;

        for (auto& it : effects)
        {
            AssertThrow(it.second != nullptr);

            if (it.second->IsEnabled())
            {
                AssertThrowMsg(it.second->GetEffectIndex() != ~0u, "Not yet initialized - index not set yet");

                post_processing_uniforms.masks[stage_index] |= 1u << it.second->GetEffectIndex();
                post_processing_uniforms.last_enabled_indices[stage_index] = MathUtil::Max(
                    post_processing_uniforms.last_enabled_indices[stage_index],
                    it.second->GetEffectIndex());
            }
        }
    }

    return post_processing_uniforms;
}

void PostProcessing::CreateUniformBuffer()
{
    Threads::AssertOnThread(g_render_thread);

    const PostProcessingUniforms post_processing_uniforms = GetUniforms();

    m_uniform_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(post_processing_uniforms));
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create());
    m_uniform_buffer->Copy(sizeof(PostProcessingUniforms), &post_processing_uniforms);
}

void PostProcessing::RenderPre(FrameBase* frame, const RenderSetup& render_setup) const
{
    Threads::AssertOnThread(g_render_thread);

    uint32 index = 0;

    for (auto& it : m_effects[uint32(POST_PROCESSING_STAGE_PRE_SHADING)])
    {
        auto& effect = it.second;

        effect->RenderEffect(frame, render_setup, index);

        ++index;
    }
}

void PostProcessing::RenderPost(FrameBase* frame, const RenderSetup& render_setup) const
{
    Threads::AssertOnThread(g_render_thread);

    uint32 index = 0;

    for (auto& it : m_effects[uint32(POST_PROCESSING_STAGE_POST_SHADING)])
    {
        auto& effect = it.second;

        effect->RenderEffect(frame, render_setup, index);

        ++index;
    }
}

HYP_DESCRIPTOR_CBUFF(View, PostProcessingUniforms, 1, sizeof(PostProcessingUniforms), false);

} // namespace hyperion
