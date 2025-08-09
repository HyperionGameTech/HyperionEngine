/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

struct alignas(16) TaaUniforms
{
    Vec2u dimensions;
    Vec2u depthTextureDimensions;
    Vec2f cameraNearFar;
};

TemporalAA::TemporalAA(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer)
    : m_inputImageView(inputImageView),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_isInitialized(false)
{
}

TemporalAA::~TemporalAA()
{
    SafeRelease(std::move(m_inputImageView));
    SafeRelease(std::move(m_uniformBuffer));
    SafeRelease(std::move(m_computePipeline));
}

void TemporalAA::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    CreateTextures();

    m_isInitialized = true;
}

void TemporalAA::CreateTextures()
{
    m_resultTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });
    m_resultTexture->SetName(NAME("TaaResult"));
    InitObject(m_resultTexture);

    m_historyTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_historyTexture->SetName(NAME("TaaHistory"));
    InitObject(m_historyTexture);
}

void TemporalAA::UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup)
{
    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_resultTexture,
        &m_historyTexture
    };

    const auto setDescriptorElements = [this, &textures](DescriptorSetBase* descriptorSet, uint32 frameIndex)
    {
        descriptorSet->SetElement("InColorTexture", m_inputImageView);
        descriptorSet->SetElement("InPrevColorTexture", g_renderBackend->GetTextureImageView((*textures[(frameIndex + 1) % 2])));

        descriptorSet->SetElement("InVelocityTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());

        descriptorSet->SetElement("InDepthTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());

        descriptorSet->SetElement("SamplerLinear", g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement("SamplerNearest", g_renderGlobalState->placeholderData->GetSamplerNearest());

        descriptorSet->SetElement("OutColorImage", g_renderBackend->GetTextureImageView((*textures[frameIndex % 2])));

        descriptorSet->SetElement("UniformBuffer", m_uniformBuffer);
    };

    if (!m_uniformBuffer)
    {
        Assert(renderSetup.HasView());

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi_GetRenderProxy(renderSetup.view->GetCamera()));
        Assert(cameraProxy != nullptr);

        m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(TaaUniforms));
        HYP_GFX_ASSERT(m_uniformBuffer->Create());

        m_uniformBuffer->SetDebugName(NAME("TaaUniforms"));

        TaaUniforms uniforms {};
        uniforms.dimensions = m_extent;
        uniforms.depthTextureDimensions = Vec2u {
            m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImage()->GetExtent().x,
            m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImage()->GetExtent().y
        };
        uniforms.cameraNearFar = Vec2f {
            cameraProxy->bufferData.cameraNear,
            cameraProxy->bufferData.cameraFar
        };

        m_uniformBuffer->Copy(sizeof(uniforms), &uniforms);
    }

    if (!m_computePipeline)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("TemporalAA"));
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            // create descriptor sets for depth pyramid generation.
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("TemporalAADescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            setDescriptorElements(descriptorSet, frameIndex);
        }

        HYP_GFX_ASSERT(descriptorTable->Create());

        m_computePipeline = g_renderBackend->MakeComputePipeline(shader, descriptorTable);
        HYP_GFX_ASSERT(m_computePipeline->Create());

        return;
    }

    const DescriptorSetRef& descriptorSet = m_computePipeline->GetDescriptorTable()->GetDescriptorSet("TemporalAADescriptorSet", frame->GetFrameIndex());
    Assert(descriptorSet != nullptr);

    setDescriptorElements(descriptorSet, frame->GetFrameIndex());
}

void TemporalAA::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Temporal AA");

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    UpdatePipelineState(frame, renderSetup);

    const uint32 frameIndex = frame->GetFrameIndex();

    const GpuImageRef& activeImage = frame->GetFrameIndex() % 2 == 0
        ? m_resultTexture->GetGpuImage()
        : m_historyTexture->GetGpuImage();

    frame->renderQueue << InsertBarrier(activeImage, RS_UNORDERED_ACCESS);

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RB_OPAQUE)
                                             .GetGBufferAttachment(GTN_DEPTH)
                                             ->GetImage()
                                             ->GetExtent();

    frame->renderQueue << BindComputePipeline(m_computePipeline);

    frame->renderQueue << BindDescriptorTable(
        m_computePipeline->GetDescriptorTable(),
        m_computePipeline,
        {},
        frameIndex);

    frame->renderQueue << DispatchCompute(m_computePipeline, Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->renderQueue << InsertBarrier(activeImage, RS_SHADER_RESOURCE);
}

} // namespace hyperion
