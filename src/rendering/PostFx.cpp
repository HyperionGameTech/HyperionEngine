#include "PostFX.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::ImageSamplerDescriptor;

PostProcessingEffect::PostProcessingEffect(
    Stage stage,
    UInt index,
    Image::InternalFormat image_format
) : EngineComponentBase(),
    m_full_screen_pass(
        nullptr,
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
    Teardown();
}

void PostProcessingEffect::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_shader = CreateShader(engine);
    m_shader.Init();

    m_full_screen_pass.SetShader(m_shader.IncRef());
    m_full_screen_pass.Create(engine);

    SetReady(true);

    OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
        SetReady(false);

        m_full_screen_pass.Destroy(engine);
        m_shader = nullptr;
    }), engine);
}

PostProcessing::PostProcessing()  = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create(Engine *engine)
{
    for (auto &effect : m_pre_effects) {
        AssertThrow(effect.second != nullptr);

        effect.second->Init(engine);
    }
    
    for (auto &effect : m_post_effects) {
        AssertThrow(effect.second != nullptr);

        effect.second->Init(engine);
    }

    CreateUniformBuffer(engine);
}

void PostProcessing::Destroy(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        descriptor_set_globals->GetDescriptor(DescriptorKey::POST_FX_UNIFORMS)
            ->RemoveSubDescriptor(0);
    }

    HYPERION_ASSERT_RESULT(m_uniform_buffer.Destroy(engine->GetDevice()));

    m_pre_effects.Clear();
    m_post_effects.Clear();
}

void PostProcessing::CreateUniformBuffer(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    HYPERION_ASSERT_RESULT(m_uniform_buffer.Create(engine->GetDevice(), sizeof(PostProcessingUniforms)));

    PostProcessingUniforms post_processing_uniforms{};

    TypeMap<std::unique_ptr<PostProcessingEffect>> *effect_passes[2] = {
        &m_pre_effects,
        &m_post_effects
    };

    for (UInt i = 0; i < static_cast<UInt>(std::size(effect_passes)); i++) {
        auto &effects = effect_passes[i];

        post_processing_uniforms.effect_counts[i]        = static_cast<UInt32>(effects->Size());
        post_processing_uniforms.masks[i]                = 0;
        post_processing_uniforms.last_enabled_indices[i] = 0;

        for (auto &it : *effects) {
            if (it.second != nullptr && it.second->IsEnabled()) {
                AssertThrowMsg(it.second->GetIndex() != ~0u, "Not yet initialized - index not set yet");

                post_processing_uniforms.masks[i] |= 1u << it.second->GetIndex();
                post_processing_uniforms.last_enabled_indices[i] = MathUtil::Max(
                    post_processing_uniforms.last_enabled_indices[i],
                    it.second->GetIndex()
                );
            }
        }
    }

    m_uniform_buffer.Copy(
        engine->GetDevice(),
        sizeof(PostProcessingUniforms),
        &post_processing_uniforms
    );

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        descriptor_set_globals->AddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::POST_FX_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer        = &m_uniform_buffer
            });
    }
}

void PostProcessing::RenderPre(Engine *engine, Frame *frame) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    UInt index = 0;

    for (auto &it : m_pre_effects) {
        auto &effect = it.second;

        effect->GetFullScreenPass().SetPushConstants({
            .post_fx_data = { index << 1 }
        });

        effect->GetFullScreenPass().Record(engine, frame->GetFrameIndex());
        effect->GetFullScreenPass().Render(engine, frame);

        ++index;
    }
}

void PostProcessing::RenderPost(Engine *engine, Frame *frame) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    UInt index = 0;

    for (auto &it : m_post_effects) {
        auto &effect = it.second;
        
        effect->GetFullScreenPass().SetPushConstants({
            .post_fx_data = { (index << 1) | 1 }
        });

        effect->GetFullScreenPass().Record(engine, frame->GetFrameIndex());
        effect->GetFullScreenPass().Render(engine, frame);

        ++index;
    }
}
} // namespace hyperion
