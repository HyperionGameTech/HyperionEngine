#include "PostFX.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;

PostFXPass::PostFXPass(InternalFormat image_format)
    : FullScreenPass(Handle<Shader>(), image_format)
{
}

PostFXPass::PostFXPass(
    const Handle<Shader> &shader,
    InternalFormat image_format
) : FullScreenPass(
        shader,
        DescriptorKey::POST_FX_PRE_STACK,
        ~0u,
        image_format
    )
{
}

PostFXPass::PostFXPass(
    const Handle<Shader> &shader,
    DescriptorKey descriptor_key,
    UInt sub_descriptor_index,
    InternalFormat image_format
) : FullScreenPass(
        shader,
        descriptor_key,
        sub_descriptor_index,
        image_format
    )
{
}

PostFXPass::~PostFXPass() = default;

void PostFXPass::CreateDescriptors()
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    if (!m_framebuffer->GetAttachmentUsages().empty()) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageDescriptor>(m_descriptor_key);

            AssertThrowMsg(m_framebuffer->GetAttachmentUsages().size() == 1, "> 1 attachments not supported currently for full screen passes");

            for (const auto *attachment_usage : m_framebuffer->GetAttachmentUsages()) {
                m_sub_descriptor_index = descriptor->SetSubDescriptor({
                    .element_index = m_sub_descriptor_index,
                    .image_view = attachment_usage->GetImageView()
                });
            }
        }
    }
}

PostProcessingEffect::PostProcessingEffect(
    Stage stage,
    UInt index,
    InternalFormat image_format
) : EngineComponentBase(),
    m_pass(
        Handle<Shader>(),
        stage == Stage::PRE_SHADING
            ? DescriptorKey::POST_FX_PRE_STACK
            : DescriptorKey::POST_FX_POST_STACK,
        index,
        image_format
    ),
    m_stage(stage),
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

    EngineComponentBase::Init();

    m_shader = CreateShader();
    InitObject(m_shader);

    m_pass.SetShader(m_shader);
    m_pass.Create();

    SetReady(true);
}

PostProcessing::PostProcessing() = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    // Fill out placeholder images -- 8 pre, 8 post.
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            for (auto descriptor_key : { DescriptorKey::POST_FX_PRE_STACK, DescriptorKey::POST_FX_POST_STACK }) {
                auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageDescriptor>(descriptor_key);

                for (UInt effect_index = 0; effect_index < 8; effect_index++) {
                    descriptor->SetElementSRV(effect_index, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
                }
            }
        }
    }

    for (UInt stage_index = 0; stage_index < 2; stage_index++) {
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
    Threads::AssertOnThread(THREAD_RENDER);

    {
        std::lock_guard guard(m_effects_mutex);

        for (UInt stage_index = 0; stage_index < 2; stage_index++) {
            m_effects_pending_addition[stage_index].Clear();
            m_effects_pending_removal[stage_index].Clear();
        }
    }

    for (UInt stage_index = 0; stage_index < 2; stage_index++) {
        for (auto &it : m_effects[stage_index]) {
            AssertThrow(it.second != nullptr);

            it.second->OnRemoved();
        }

        m_effects[stage_index].Clear();
    }

    for (const auto descriptor_set_index : DescriptorSet::global_buffer_mapping) {
        auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(descriptor_set_index);

        descriptor_set->GetDescriptor(DescriptorKey::POST_FX_UNIFORMS)
            ->RemoveSubDescriptor(0);

        // remove all images
        for (const auto descriptor_key : { DescriptorKey::POST_FX_PRE_STACK, DescriptorKey::POST_FX_POST_STACK }) {
            descriptor_set->RemoveDescriptor(descriptor_key);
        }
    }

    SafeRelease(std::move(m_uniform_buffer));
}

void PostProcessing::PerformUpdates()
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!m_effects_updated.load()) {
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

    m_effects_updated.store(false);

    HYP_SYNC_RENDER();
}

PostProcessingUniforms PostProcessing::GetUniforms() const
{
    PostProcessingUniforms post_processing_uniforms { };

    for (UInt stage_index = 0; stage_index < 2; stage_index++) {
        auto &effects = m_effects[stage_index];

        post_processing_uniforms.effect_counts[stage_index] = UInt32(effects.Size());
        post_processing_uniforms.masks[stage_index] = 0u;
        post_processing_uniforms.last_enabled_indices[stage_index] = 0u;

        for (auto &it : effects) {
            AssertThrow(it.second != nullptr);

            if (it.second->IsEnabled()) {
                AssertThrowMsg(it.second->GetIndex() != ~0u, "Not yet initialized - index not set yet");

                post_processing_uniforms.masks[stage_index] |= 1u << it.second->GetIndex();
                post_processing_uniforms.last_enabled_indices[stage_index] = MathUtil::Max(
                    post_processing_uniforms.last_enabled_indices[stage_index],
                    it.second->GetIndex()
                );
            }
        }
    }

    return post_processing_uniforms;
}

void PostProcessing::CreateUniformBuffer()
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    const PostProcessingUniforms post_processing_uniforms = GetUniforms();

    m_uniform_buffer = RenderObjects::Make<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER);
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create(Engine::Get()->GetGPUDevice(), sizeof(post_processing_uniforms)));

    m_uniform_buffer->Copy(
        Engine::Get()->GetGPUDevice(),
        sizeof(PostProcessingUniforms),
        &post_processing_uniforms
    );

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::POST_FX_UNIFORMS)
            ->SetElementBuffer(0, m_uniform_buffer);
    }
}

void PostProcessing::RenderPre(Frame *frame) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    UInt index = 0;

    for (auto &it : m_effects[UInt(PostProcessingEffect::Stage::PRE_SHADING)]) {
        auto &effect = it.second;

        effect->GetPass().SetPushConstants({
            .post_fx_data = { index << 1 }
        });

        effect->GetPass().Record(frame->GetFrameIndex());
        effect->GetPass().Render(frame);

        ++index;
    }
}

void PostProcessing::RenderPost(Frame *frame) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    UInt index = 0;

    for (auto &it : m_effects[UInt(PostProcessingEffect::Stage::POST_SHADING)]) {
        auto &effect = it.second;
        
        effect->GetPass().SetPushConstants({
            .post_fx_data = { (index << 1) | 1 }
        });

        effect->GetPass().Record(frame->GetFrameIndex());
        effect->GetPass().Render(frame);

        ++index;
    }
}
} // namespace hyperion
