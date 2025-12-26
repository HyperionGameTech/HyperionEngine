/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/PostFX.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/DescriptorSet.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <util/MeshBuilder.hpp>

#include <PostFX.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

PostFXPass::PostFXPass(TextureFormat imageFormat, GBuffer* gbuffer)
    : PostFXPass(nullptr, POST_PROCESSING_STAGE_PRE_SHADING, ~0u, imageFormat, gbuffer)
{
}

PostFXPass::PostFXPass(const ShaderRef& shader, TextureFormat imageFormat, GBuffer* gbuffer)
    : PostFXPass(shader, POST_PROCESSING_STAGE_PRE_SHADING, ~0u, imageFormat, gbuffer)
{
}

PostFXPass::PostFXPass(const ShaderRef& shader, PostProcessingStage stage, uint32 effectIndex, TextureFormat imageFormat, GBuffer* gbuffer)
    : FullScreenPass(shader, imageFormat, Vec2u::Zero(), gbuffer),
      m_stage(stage),
      m_effectIndex(effectIndex)
{
}

PostFXPass::~PostFXPass()
{
}

void PostFXPass::CreateDescriptors()
{
    AssertOnThread(g_renderThread);

    if (m_effectIndex == ~0u)
    {
        HYP_LOG(Rendering, Warning, "Effect index not set, skipping descriptor creation");

        return;
    }

    if (!g_renderBackend->GetRenderConfig().dynamicDescriptorIndexing)
    {
        HYP_LOG(Rendering, Warning, "Creating post processing pass on a device that does not support dynamic descriptor indexing");

        return;
    }

    /// \todo Reimplement

    // const Name descriptorName = m_stage == POST_PROCESSING_STAGE_PRE_SHADING
    //     ? NAME("PostFXPreStack")
    //     : NAME("PostFXPostStack");

    // for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++) {
    //     g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
    //         ->SetElement(descriptorName, m_effectIndex, m_framebuffer->GetAttachment(0)->GetImageView());
    // }
}

PostProcessingEffect::PostProcessingEffect(PostProcessingStage stage, uint32 effectIndex, TextureFormat imageFormat, GBuffer* gbuffer)
    : m_pass(nullptr, stage, effectIndex, imageFormat, gbuffer),
      m_isEnabled(true)
{
}

PostProcessingEffect::~PostProcessingEffect() = default;

void PostProcessingEffect::Init()
{
    m_shader = CreateShader();

    m_pass.SetShader(m_shader);
    m_pass.Create();
}

void PostProcessingEffect::RenderEffect(Frame* frame, const RenderSetup& renderSetup, uint32 slot)
{
    struct
    {
        uint32 currentEffectIndexStage; // 31bits for index, 1 bit for stage
    } pushConstants;

    pushConstants.currentEffectIndexStage = (slot << 1) | uint32(m_pass.GetStage());

    m_pass.SetPushConstants(&pushConstants, sizeof(pushConstants));
    m_pass.Render(frame, renderSetup);
}

PostProcessing::PostProcessing() = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create()
{
    AssertOnThread(g_renderThread);

    for (uint32 stageIndex = 0; stageIndex < 2; stageIndex++)
    {
        for (auto& effect : m_effects[stageIndex])
        {
            Assert(effect.second != nullptr);

            effect.second->Init();

            effect.second->OnAdded();
        }
    }

    CreateUniformBuffer();

    PerformUpdates();
}

void PostProcessing::Destroy()
{
    AssertOnThread(g_renderThread);

    {
        std::lock_guard guard(m_effectsMutex);

        for (uint32 stageIndex = 0; stageIndex < 2; stageIndex++)
        {
            m_effectsPendingAddition[stageIndex].Clear();
            m_effectsPendingRemoval[stageIndex].Clear();
        }
    }

    for (uint32 stageIndex = 0; stageIndex < 2; stageIndex++)
    {
        for (auto& it : m_effects[stageIndex])
        {
            Assert(it.second != nullptr);

            it.second->OnRemoved();
        }

        m_effects[stageIndex].Clear();
    }

    SafeDelete(std::move(m_uniformBuffer));
}

void PostProcessing::PerformUpdates()
{
    AssertOnThread(g_renderThread);

    if (!m_effectsUpdated.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    std::lock_guard guard(m_effectsMutex);

    for (SizeType stageIndex = 0; stageIndex < 2; stageIndex++)
    {
        for (auto& it : m_effectsPendingAddition[stageIndex])
        {
            const TypeId typeId = it.first;
            auto& effect = it.second;

            Assert(effect != nullptr);

            effect->Init();

            effect->OnAdded();

            m_effects[stageIndex].Set(typeId, std::move(effect));
        }

        m_effectsPendingAddition[stageIndex].Clear();

        for (const TypeId typeId : m_effectsPendingRemoval[stageIndex])
        {
            const auto effectsIt = m_effects[stageIndex].Find(typeId);

            if (effectsIt != m_effects[stageIndex].End())
            {
                Assert(effectsIt->second != nullptr);

                effectsIt->second->OnRemoved();

                m_effects[stageIndex].Erase(effectsIt);
            }
        }

        m_effectsPendingRemoval[stageIndex].Clear();
    }

    m_effectsUpdated.Set(false, MemoryOrder::RELEASE);

    HYP_SYNC_RENDER();
}

PostProcessingUniforms PostProcessing::GetUniforms() const
{
    PostProcessingUniforms postProcessingUniforms {};

    for (uint32 stageIndex = 0; stageIndex < 2; stageIndex++)
    {
        auto& effects = m_effects[stageIndex];

        postProcessingUniforms.effectCounts[stageIndex] = uint32(effects.Size());
        postProcessingUniforms.masks[stageIndex] = 0u;
        postProcessingUniforms.lastEnabledIndices[stageIndex] = 0u;

        for (auto& it : effects)
        {
            Assert(it.second != nullptr);

            if (it.second->IsEnabled())
            {
                Assert(it.second->GetEffectIndex() != ~0u, "Not yet initialized - index not set yet");

                postProcessingUniforms.masks[stageIndex] |= 1u << it.second->GetEffectIndex();
                postProcessingUniforms.lastEnabledIndices[stageIndex] = MathUtil::Max(
                    postProcessingUniforms.lastEnabledIndices[stageIndex],
                    it.second->GetEffectIndex());
            }
        }
    }

    return postProcessingUniforms;
}

void PostProcessing::CreateUniformBuffer()
{
    AssertOnThread(g_renderThread);

    const PostProcessingUniforms postProcessingUniforms = GetUniforms();

    m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(postProcessingUniforms));
    HYP_GFX_ASSERT(m_uniformBuffer->Create());
    m_uniformBuffer->Copy(sizeof(PostProcessingUniforms), &postProcessingUniforms);
}

void PostProcessing::RenderPre(Frame* frame, const RenderSetup& renderSetup) const
{
    AssertOnThread(g_renderThread);

    uint32 index = 0;

    for (auto& it : m_effects[uint32(POST_PROCESSING_STAGE_PRE_SHADING)])
    {
        auto& effect = it.second;

        effect->RenderEffect(frame, renderSetup, index);

        ++index;
    }
}

void PostProcessing::RenderPost(Frame* frame, const RenderSetup& renderSetup) const
{
    AssertOnThread(g_renderThread);

    uint32 index = 0;

    for (auto& it : m_effects[uint32(POST_PROCESSING_STAGE_POST_SHADING)])
    {
        auto& effect = it.second;

        effect->RenderEffect(frame, renderSetup, index);

        ++index;
    }
}

HYP_DESCRIPTOR_CBUFF(View, PostProcessingUniforms, 1, sizeof(PostProcessingUniforms), false);

} // namespace Hyperion
