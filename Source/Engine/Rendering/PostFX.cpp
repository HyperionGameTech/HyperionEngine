/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/PostFX.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/DescriptorSet.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <Rendering/util/MeshBuilder.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

PostFXPass::PostFXPass(TextureFormat imageFormat, GBuffer* gbuffer)
    : PostFXPass(ShaderDesc(), POST_PROCESSING_STAGE_PRE_SHADING, ~0u, imageFormat, gbuffer)
{
}

PostFXPass::PostFXPass(
    const ShaderDesc& shaderDesc,
    TextureFormat imageFormat,
    GBuffer* gbuffer)
    : PostFXPass(shaderDesc, POST_PROCESSING_STAGE_PRE_SHADING, ~0u, imageFormat, gbuffer)
{
}

PostFXPass::PostFXPass(
    const ShaderDesc& shaderDesc,
    PostProcessingStage stage,
    uint32 effectIndex,
    TextureFormat imageFormat,
    GBuffer* gbuffer)
    : FullScreenPass(shaderDesc, imageFormat, Vec2u::Zero(), gbuffer),
      m_stage(stage),
      m_effectIndex(effectIndex)
{
    SetPassName(NAME("PostFX"));
}

PostFXPass::~PostFXPass()
{
}

PostProcessingEffect::PostProcessingEffect(PostProcessingStage stage, uint32 effectIndex, TextureFormat imageFormat, GBuffer* gbuffer)
    : m_pass(ShaderDesc(), stage, effectIndex, imageFormat, gbuffer),
      m_isEnabled(true)
{
}

PostProcessingEffect::~PostProcessingEffect() = default;

void PostProcessingEffect::Initialize()
{
    m_shaderDesc = GetShaderDesc();

    m_pass.SetShaderDesc(m_shaderDesc);
    m_pass.Create();
}

void PostProcessingEffect::RenderEffect(Frame* frame, const RenderSetup& renderSetup, uint32 slot)
{
    //struct
    //{
    //    uint32 currentEffectIndexStage; // 31bits for index, 1 bit for stage
    //} pushConstants;

    //pushConstants.currentEffectIndexStage = (slot << 1) | uint32(m_pass.GetStage());

    //m_pass.SetPushConstants(&pushConstants, sizeof(pushConstants));
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

            effect.second->Initialize();

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

    EnqueueDeletion(std::move(m_uniformBuffer));
}

void PostProcessing::PerformUpdates()
{
    AssertOnThread(g_renderThread);

    if (!m_effectsUpdated.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    std::lock_guard guard(m_effectsMutex);

    for (size_t stageIndex = 0; stageIndex < 2; stageIndex++)
    {
        for (auto& it : m_effectsPendingAddition[stageIndex])
        {
            const TypeId typeId = it.first;
            auto& effect = it.second;

            Assert(effect != nullptr);

            effect->Initialize();

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

    m_uniformBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(postProcessingUniforms));
    CheckResult(m_uniformBuffer->Create());

    m_uniformBuffer->Copy(sizeof(postProcessingUniforms), &postProcessingUniforms);
    m_uniformBuffer->Flush(0, sizeof(postProcessingUniforms));
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

} // namespace Hyperion
