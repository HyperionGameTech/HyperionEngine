/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

static const ShaderPropertyId s_propOutputRGBA8 = InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA8")));
static const ShaderPropertyId s_propOutputRGBA16F = InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA16F")));
static const ShaderPropertyId s_propOutputRGBA32F = InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA32F")));

struct TemporalBlendingConstants
{
    Vec2u outputDimensions;
    Vec2u depthTextureDimensions;
    uint32 blendingFrameCounter;
};

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TemporalBlendTechnique technique,
    double feedback,
    const GpuImageViewRef& inputImageView,
    GBuffer* gbuffer)
    : TemporalBlending(
          extent,
          TextureFormat::RGBA8,
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
    EnqueueDeletion(std::move(m_cbuffers));
    EnqueueDeletion(std::move(m_inputFramebuffer));
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
        CheckResult(m_inputFramebuffer->Create());
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
    // @TODO Use FullScreenPass to have proper sync

    if (IsOnThread(g_renderThread))
    {
        Resize_Internal(newSize);
        return;
    }

    GetThreadById(g_renderThread)->GetScheduler().Enqueue([this, newSize]()
        {
            Resize_Internal(newSize);
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
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
}

void TemporalBlending::GetShaderProperties(ShaderPropertySet& outProperties) const
{
    switch (m_imageFormat)
    {
    case TextureFormat::RGBA8:
        outProperties.Add(s_propOutputRGBA8);
        break;
    case TextureFormat::RGBA16F:
        outProperties.Add(s_propOutputRGBA16F);
        break;
    case TextureFormat::RGBA32F:
        outProperties.Add(s_propOutputRGBA32F);
        break;
    default:
        HYP_NOT_IMPLEMENTED();
    }

    outProperties.Add(InternShaderProperty(ShaderProperty(NAME("TEMPORAL_BLEND_TECHNIQUE"), int(m_technique))));
    outProperties.Add(InternShaderProperty(ShaderProperty(NAME("FEEDBACK"), float(m_feedback))));
}

void TemporalBlending::CreateImages()
{
    m_resultTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });

    m_resultTexture->SetName(NAME("TemporalBlendingResult"));
    CheckResult(m_resultTexture->Create());

    m_historyTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });

    m_historyTexture->SetName(NAME("TemporalBlendingHistory"));
    CheckResult(m_historyTexture->Create());
}

void TemporalBlending::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    // Get active image and extent
    const Handle<Texture>& activeTexture = frame->GetFrameIndex() % 2 == 0
        ? m_resultTexture
        : m_historyTexture;

    const Handle<Texture>& prevTexture = frame->GetFrameIndex() % 2 == 0
        ? m_historyTexture
        : m_resultTexture;

    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    const Vec3u& extent = activeTexture->GetExtent();

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetGpuImage()->GetExtent();

    GpuBufferRef& cbuffer = m_cbuffers[frame->GetFrameIndex()];

    if (!cbuffer)
    {
        cbuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(TemporalBlendingConstants));
#if HYP_DEBUG_MODE
        cbuffer->SetDebugName(NAME("TemporalBlendingConstants"));
#endif

        cbuffer->Create();
    }

    TemporalBlendingConstants uniforms {};
    uniforms.outputDimensions = Vec2u { extent.x, extent.y };
    uniforms.depthTextureDimensions = Vec2u { depthTextureDimensions.x, depthTextureDimensions.y };
    uniforms.blendingFrameCounter = m_blendingFrameCounter;
    cbuffer->Copy(sizeof(uniforms), &uniforms);

    ShaderPropertySet shaderProperties;
    GetShaderProperties(shaderProperties);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("TemporalBlending"), shaderProperties));

    const GpuImageViewRef& inputImageView = m_inputFramebuffer.IsValid()
            ? m_inputFramebuffer->GetAttachment(0)->GetImageView()
            : m_inputImageView;

    frame->cr << SetShaderUniform(0, "InImage"_sh, inputImageView);
    frame->cr << SetShaderUniform(1, "PrevImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(prevTexture));
    frame->cr << SetShaderUniform(2, "VelocityImage"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
    frame->cr << SetShaderUniform(3, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    frame->cr << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(5, "OutImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(activeTexture));
    frame->cr << SetShaderUniform(6, "TemporalBlendingUniforms"_sh, cbuffer);
    
    frame->cr << SetShaderUniform(7, "GBufferDepthTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView());

    frame->cr << SetShaderUniform(8, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
    frame->cr << SetShaderUniform(9, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

    frame->cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });
    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_SHADER_RESOURCE);

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
