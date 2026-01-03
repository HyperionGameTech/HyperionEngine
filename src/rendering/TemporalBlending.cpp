/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/TemporalBlending.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <core/threading/Threads.hpp>

namespace Hyperion {

struct TemporalBlendingUniforms
{
    Vec2u outputDimensions;
    Vec2u depthTextureDimensions;
    uint32 blendingFrameCounter;
};

#pragma region Render commands

struct RecreateTemporalBlendingFramebuffer : RenderCommand
{
    TemporalBlending& temporalBlending;
    Vec2u newSize;

    RecreateTemporalBlendingFramebuffer(TemporalBlending& temporalBlending, const Vec2u& newSize)
        : temporalBlending(temporalBlending),
          newSize(newSize)
    {
    }

    virtual ~RecreateTemporalBlendingFramebuffer() override = default;

    virtual RendererResult operator()() override
    {
        temporalBlending.Resize_Internal(newSize);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TemporalBlendTechnique technique,
    double feedback,
    const GpuImageViewRef& inputImageView,
    GBuffer* gbuffer)
    : TemporalBlending(
          extent,
          TF_RGBA8,
          technique,
          feedback,
          inputImageView,
          gbuffer)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TextureFormat imageFormat,
    TemporalBlendTechnique technique,
    double feedback,
    const FramebufferRef& inputFramebuffer,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_imageFormat(imageFormat),
      m_technique(technique),
      m_feedback(feedback),
      m_inputFramebuffer(inputFramebuffer),
      m_gbuffer(gbuffer),
      m_blendingFrameCounter(0),
      m_isInitialized(false)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TextureFormat imageFormat,
    TemporalBlendTechnique technique,
    double feedback,
    const GpuImageViewRef& inputImageView,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_imageFormat(imageFormat),
      m_technique(technique),
      m_feedback(feedback),
      m_inputImageView(inputImageView),
      m_gbuffer(gbuffer),
      m_blendingFrameCounter(0),
      m_isInitialized(false)
{
}

TemporalBlending::~TemporalBlending()
{
    SafeDelete(std::move(m_uniformBuffers));
    SafeDelete(std::move(m_inputFramebuffer));
    SafeDelete(std::move(m_csPerformBlending));
}

void TemporalBlending::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    if (m_inputFramebuffer.IsValid())
    {
        DeferCreate(m_inputFramebuffer);
    }

    CreateImages();

    m_onGbufferResolutionChanged = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u newSize)
        {
            Resize_Internal(newSize);
        });

    m_isInitialized = true;
}

void TemporalBlending::Resize(Vec2u newSize)
{
    PUSH_RENDER_COMMAND(RecreateTemporalBlendingFramebuffer, *this, newSize);
    HYP_SYNC_RENDER();
}

void TemporalBlending::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (m_extent == newSize)
    {
        return;
    }

    m_extent = newSize;

    if (!m_isInitialized)
    {
        return;
    }

    CreateImages();

    SafeDelete(std::move(m_csPerformBlending));
}

void TemporalBlending::GetShaderProperties(ShaderProperties& outProperties) const
{
    switch (m_imageFormat)
    {
    case TF_RGBA8:
        outProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA8")));
        break;
    case TF_RGBA16F:
        outProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA16F")));
        break;
    case TF_RGBA32F:
        outProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA32F")));
        break;
    default:
        HYP_NOT_IMPLEMENTED();
    }

    static const Name s_feedbackTypes[] = { NAME("LOW"), NAME("MEDIUM"), NAME("HIGH") };

    outProperties.Set(ShaderProperty(NAME("TEMPORAL_BLEND_TECHNIQUE"), int(m_technique)));
    outProperties.Set(ShaderProperty(NAME("FEEDBACK"), float(m_feedback)));
}

void TemporalBlending::CreateImages()
{
    m_resultTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_resultTexture->SetName(NAME("TemporalBlendingResult"));
    InitObject(m_resultTexture);

    m_historyTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_historyTexture->SetName(NAME("TemporalBlendingHistory"));
    InitObject(m_historyTexture);
}

void TemporalBlending::CreatePipeline()
{
    SafeDelete(std::move(m_csPerformBlending));

    ShaderProperties shaderProperties;
    GetShaderProperties(shaderProperties);

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("TemporalBlending"), shaderProperties);
    Assert(shader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_resultTexture,
        &m_historyTexture
    };

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        if (!m_uniformBuffers[frameIndex])
        {
            m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(
                GpuBufferType::CBUFF,
                sizeof(TemporalBlendingUniforms));

            HYP_GFX_ASSERT(m_uniformBuffers[frameIndex]->Create());
        }

        const GpuImageViewRef& inputImageView = m_inputFramebuffer.IsValid()
            ? m_inputFramebuffer->GetAttachment(0)->GetImageView()
            : m_inputImageView;

        Assert(inputImageView != nullptr);

        // input image
        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("InImage"_sh, inputImageView);

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("PrevImage"_sh, g_renderBackend->GetTextureImageView((*textures[(frameIndex + 1) % 2])));

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("VelocityImage"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("OutImage"_sh, g_renderBackend->GetTextureImageView((*textures[frameIndex % 2])));

        descriptorTable->GetDescriptorSet("TemporalBlendingDescriptorSet"_sh, frameIndex)
            ->SetElement("TemporalBlendingUniforms"_sh, m_uniformBuffers[frameIndex]);
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    m_csPerformBlending = g_renderBackend->MakeComputePipeline(shader, descriptorTable);
    HYP_GFX_ASSERT(m_csPerformBlending->Create());
}

void TemporalBlending::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    if (!m_csPerformBlending)
    {
        CreatePipeline();

        Assert(m_csPerformBlending != nullptr && m_csPerformBlending->IsCreated());
    }

    const GpuImageRef& activeImage = frame->GetFrameIndex() % 2 == 0
        ? m_resultTexture->GetGpuImage()
        : m_historyTexture->GetGpuImage();

    frame->renderQueue << InsertBarrier(activeImage, RS_UNORDERED_ACCESS);

    const Vec3u& extent = activeImage->GetExtent();

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RB_OPAQUE)
                                             .GetGBufferAttachment(GTN_DEPTH)
                                             ->GetImage()
                                             ->GetExtent();

    // Copy uniform data to gpu buffer
    TemporalBlendingUniforms uniforms {};
    uniforms.outputDimensions = Vec2u { extent.x, extent.y };
    uniforms.depthTextureDimensions = Vec2u { depthTextureDimensions.x, depthTextureDimensions.y };
    uniforms.blendingFrameCounter = m_blendingFrameCounter;
    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);

    frame->renderQueue << BindComputePipeline(m_csPerformBlending);

    frame->renderQueue << BindDescriptorTable(
        m_csPerformBlending->GetDescriptorTable(),
        m_csPerformBlending,
        { { "Global"_sh, { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
        frame->GetFrameIndex());

    const uint32 viewDescriptorSetIndex = m_csPerformBlending->GetDescriptorTable()->GetDescriptorSetIndex("View"_sh);

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            m_csPerformBlending,
            {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue << DispatchCompute(m_csPerformBlending, Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    // set it to be able to be used as texture2D for next pass, or outside of this
    frame->renderQueue << InsertBarrier(activeImage, RS_SHADER_RESOURCE);

    m_blendingFrameCounter = m_technique == TemporalBlendTechnique::TECHNIQUE_4
        ? m_blendingFrameCounter + 1
        : 0;
}

void TemporalBlending::ResetProgressiveBlending()
{
    // roll over to 0 on next increment to add an extra frame
    m_blendingFrameCounter = MathUtil::MaxSafeValue(m_blendingFrameCounter);
}

} // namespace Hyperion
