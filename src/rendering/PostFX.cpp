/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PostFX.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion {


PostFXPass::PostFXPass(InternalFormat image_format)
    : FullScreenPass(nullptr, image_format)
{
}

PostFXPass::PostFXPass(
    const ShaderRef &shader,
    InternalFormat image_format
) : PostFXPass(
        shader,
        POST_PROCESSING_STAGE_PRE_SHADING,
        ~0u,
        image_format
    )
{
}

PostFXPass::PostFXPass(
    const ShaderRef &shader,
    PostProcessingStage stage,
    uint effect_index,
    InternalFormat image_format
) : FullScreenPass(
        shader,
        image_format
    ),
    m_stage(stage),
    m_effect_index(effect_index)
{
}

PostFXPass::~PostFXPass()
{
}

void PostFXPass::CreateDescriptors()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_framebuffer->GetAttachmentMap()->Size() != 0) {
        if (m_effect_index == ~0u) {
            DebugLog(LogType::Warn, "Effect index not set, skipping descriptor creation\n");

            return;
        }

        const Name descriptor_name = m_stage == POST_PROCESSING_STAGE_PRE_SHADING
            ? NAME("PostFXPreStack")
            : NAME("PostFXPostStack");

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(descriptor_name, m_effect_index, m_framebuffer->GetAttachment(0)->GetImageView());
        }
    }
}

PostProcessingEffect::PostProcessingEffect(
    PostProcessingStage stage,
    uint effect_index,
    InternalFormat image_format
) : BasicObject(),
    m_pass(
        nullptr,
        stage,
        effect_index,
        image_format
    ),
    m_is_enabled(true)
{
}

PostProcessingEffect::~PostProcessingEffect()
{
    if (IsInitCalled()) {
        SetReady(false);

        m_pass.Destroy();
        m_shader.Reset();

        HYP_SYNC_RENDER();
    }
}

void PostProcessingEffect::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_shader = CreateShader();

    m_pass.SetShader(m_shader);
    m_pass.Create();

    SetReady(true);
}

void PostProcessingEffect::RenderEffect(Frame *frame, uint slot)
{
    struct alignas(128)
    {
        uint32  current_effect_index_stage; // 31bits for index, 1 bit for stage
    } push_constants;

    push_constants.current_effect_index_stage = (slot << 1) | uint32(m_pass.GetStage());

    m_pass.SetPushConstants(&push_constants, sizeof(push_constants));
    
    m_pass.Record(frame->GetFrameIndex());
    m_pass.Render(frame);
}

PostProcessing::PostProcessing() = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    for (uint stage_index = 0; stage_index < 2; stage_index++) {
        for (auto &effect : m_effects[stage_index]) {
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    {
        std::lock_guard guard(m_effects_mutex);

        for (uint stage_index = 0; stage_index < 2; stage_index++) {
            m_effects_pending_addition[stage_index].Clear();
            m_effects_pending_removal[stage_index].Clear();
        }
    }

    for (uint stage_index = 0; stage_index < 2; stage_index++) {
        for (auto &it : m_effects[stage_index]) {
            AssertThrow(it.second != nullptr);

            it.second->OnRemoved();
        }

        m_effects[stage_index].Clear();
    }

    SafeRelease(std::move(m_uniform_buffer));
}

void PostProcessing::PerformUpdates()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (!m_effects_updated.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    std::lock_guard guard(m_effects_mutex);

    for (SizeType stage_index = 0; stage_index < 2; stage_index++) {
        for (auto &it : m_effects_pending_addition[stage_index]) {
            const TypeID type_id = it.first;
            auto &effect = it.second;

            AssertThrow(effect != nullptr);

            effect->Init();

            effect->OnAdded();

            m_effects[stage_index].Set(type_id, std::move(effect));
        }

        m_effects_pending_addition[stage_index].Clear();

        for (const TypeID type_id : m_effects_pending_removal[stage_index]) {
            const auto effects_it = m_effects[stage_index].Find(type_id);

            if (effects_it != m_effects[stage_index].End()) {
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
    PostProcessingUniforms post_processing_uniforms { };

    for (uint stage_index = 0; stage_index < 2; stage_index++) {
        auto &effects = m_effects[stage_index];

        post_processing_uniforms.effect_counts[stage_index] = uint32(effects.Size());
        post_processing_uniforms.masks[stage_index] = 0u;
        post_processing_uniforms.last_enabled_indices[stage_index] = 0u;

        for (auto &it : effects) {
            AssertThrow(it.second != nullptr);

            if (it.second->IsEnabled()) {
                AssertThrowMsg(it.second->GetEffectIndex() != ~0u, "Not yet initialized - index not set yet");

                post_processing_uniforms.masks[stage_index] |= 1u << it.second->GetEffectIndex();
                post_processing_uniforms.last_enabled_indices[stage_index] = MathUtil::Max(
                    post_processing_uniforms.last_enabled_indices[stage_index],
                    it.second->GetEffectIndex()
                );
            }
        }
    }

    return post_processing_uniforms;
}

void PostProcessing::CreateUniformBuffer()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const PostProcessingUniforms post_processing_uniforms = GetUniforms();

    m_uniform_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER);
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(post_processing_uniforms)));

    m_uniform_buffer->Copy(
        g_engine->GetGPUDevice(),
        sizeof(PostProcessingUniforms),
        &post_processing_uniforms
    );

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("PostProcessingUniforms"), m_uniform_buffer);
    }
}

void PostProcessing::RenderPre(Frame *frame) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    uint index = 0;

    for (auto &it : m_effects[uint(POST_PROCESSING_STAGE_PRE_SHADING)]) {
        auto &effect = it.second;

        effect->RenderEffect(frame, index);

        ++index;
    }
}

void PostProcessing::RenderPost(Frame *frame) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    uint index = 0;

    for (auto &it : m_effects[uint(POST_PROCESSING_STAGE_POST_SHADING)]) {
        auto &effect = it.second;

        effect->RenderEffect(frame, index);

        ++index;
    }
}
} // namespace hyperion
