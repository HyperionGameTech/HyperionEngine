/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <FullScreenPass.generated.inl>

namespace Hyperion {

static const ShaderPropertyId s_propHalfRes = InternShaderProperty(ShaderProperty(NAME("HALFRES")));

struct MergeHalfResTexturesUniforms
{
    Vec2u dimensions;
};

#pragma region Render commands

struct RecreateFullScreenPassFramebuffer : RenderCommand
{
    WeakHandle<FullScreenPass> fullScreenPassWeak;
    Vec2u newSize;

    RecreateFullScreenPassFramebuffer(const WeakHandle<FullScreenPass>& fullScreenPassWeak, Vec2u newSize)
        : fullScreenPassWeak(fullScreenPassWeak),
          newSize(newSize)
    {
    }

    virtual ~RecreateFullScreenPassFramebuffer() override = default;

    virtual RendererResult operator()() override
    {
        Handle<FullScreenPass> fullScreenPass = fullScreenPassWeak.Lock();
        if (!fullScreenPass)
        {
            HYP_LOG(Rendering, Debug, "FullScreenPass {} is no longer alive, skipping recreate.", fullScreenPassWeak.Id());

            return {};
        }

        if (fullScreenPass->m_isInitialized)
        {
            fullScreenPass->Resize_Internal(newSize);
        }
        else
        {
            fullScreenPass->m_extent = newSize;
        }

        return {};
    }
};

#pragma endregion Render commands

FullScreenPass::FullScreenPass(EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(InvalidTextureFormat, nullptr, flags)
{
}

FullScreenPass::FullScreenPass(TextureFormat imageFormat, GBuffer* gbuffer, EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(ShaderDesc(), imageFormat, Vec2u::Zero(), gbuffer, flags)
{
}

FullScreenPass::FullScreenPass(TextureFormat imageFormat, Vec2u extent, GBuffer* gbuffer, EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(ShaderDesc(), imageFormat, extent, gbuffer, flags)
{
}

FullScreenPass::FullScreenPass(
    const ShaderDesc& shaderDesc,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer,
    EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(
          shaderDesc,
          FramebufferRef::Null(),
          imageFormat,
          extent,
          gbuffer,
          flags)
{
}

FullScreenPass::FullScreenPass(
    const ShaderDesc& shaderDesc,
    const FramebufferRef& framebuffer,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer,
    EnumFlags<FullScreenPassFlags> flags)
    : m_shaderDesc(shaderDesc),
      m_framebuffer(framebuffer),
      m_imageFormat(imageFormat),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_flags(flags),
      m_blendFunction(BlendFunction::None()),
      m_isInitialized(false),
      m_isFirstFrame(true)
{
}

FullScreenPass::~FullScreenPass()
{
    m_fullScreenQuad.Reset();

    EnqueueDeletion(std::move(m_framebuffer));
    EnqueueDeletion(std::move(m_mergeHalfResTexturesUniformBuffer));

    // not calling EnqueueDeletion() for graphics pipeline as it is managed by the graphics pipeline caching system
}

const GpuImageViewRef& FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending())
    {
        Assert(m_temporalBlending != nullptr);

        return g_renderInterface->textureViewCache->GetOrCreate(m_temporalBlending->GetResultTexture());
    }

    if (ShouldRenderHalfRes())
    {
        return m_mergeHalfResTexturesPass->GetFinalImageView();
    }

    AttachmentBase* colorAttachment = GetAttachment(0);

    if (!colorAttachment)
    {
        return GpuImageViewRef::Null();
    }

    return colorAttachment->GetImageView();
}

const GpuImageViewRef& FullScreenPass::GetPreviousFrameColorImageView() const
{
    // If we're rendering at half res, we use the same image we render to but at an offset.
    if (ShouldRenderHalfRes())
    {
        AttachmentBase* colorAttachment = GetAttachment(0);

        if (!colorAttachment)
        {
            return GpuImageViewRef::Null();
        }

        return colorAttachment->GetImageView();
    }

    if (m_historyTexture.IsValid())
    {
        return g_renderInterface->textureViewCache->GetOrCreate(m_historyTexture);
    }

    return GpuImageViewRef::Null();
}

void FullScreenPass::Create()
{
    HYP_SCOPE;

    Assert(!m_isInitialized);

    CreateFullScreenQuad();
    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateTemporalBlending();

    /// @NOTE: if we use temporal blending and we're not rendering at half res, we need a history texture,
    ///        so we blit to it each frame after rendering and use it as input the next frame.
    ///        (when using halfres, the merge pass handles history internally via checkerboarding)
    if (UsesTemporalBlending() && !ShouldRenderHalfRes())
    {
        CreateHistoryTexture();
    }

    m_isInitialized = true;
}

void FullScreenPass::SetShaderDesc(const ShaderDesc& shaderDesc)
{
    m_shaderDesc = shaderDesc;
}

AttachmentBase* FullScreenPass::GetAttachment(uint32 attachmentIndex) const
{
    Assert(m_framebuffer.IsValid());

    return m_framebuffer->GetAttachment(attachmentIndex);
}

void FullScreenPass::SetBlendFunction(const BlendFunction& blendFunction)
{
    if (m_blendFunction == blendFunction)
    {
        return;
    }

    m_blendFunction = blendFunction;
}

void FullScreenPass::Resize(Vec2u newSize)
{
    PUSH_RENDER_COMMAND(RecreateFullScreenPassFramebuffer, WeakHandleFromThis(), newSize);
}

void FullScreenPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (m_extent == newSize)
    {
        return;
    }

    AssertDebug(newSize.Volume() != 0, "Cannot resize FullScreenPass to zero size!");

    newSize = MathUtil::Max(newSize, Vec2u::One());

    HYP_LOG(Rendering, Debug, "Resizing FullScreenPass {} from {} to {}",
        Id().Value(),
        m_extent,
        newSize);

    m_extent = newSize;

    if (!m_isInitialized)
    {
        // not yet created, just set new size
        return;
    }

    if (!(m_flags & FSP_EXTERNAL_RENDERTARGET))
    {
        if (!m_framebuffer || m_framebuffer->GetExtent() == newSize)
        {
            EnqueueDeletion(std::move(m_framebuffer));
            CreateFramebuffer();
        }
    }

    m_temporalBlending.Reset();

    CreateMergeHalfResTexturesPass();
    CreateTemporalBlending();

    if (UsesTemporalBlending() && !ShouldRenderHalfRes())
    {
        CreateHistoryTexture();
    }
}

void FullScreenPass::CreateFullScreenQuad()
{
    HYP_SCOPE;

    m_fullScreenQuad = MeshBuilder::Quad();
    m_fullScreenQuad->SetFlags(MF_VIEW_INDEPENDENT);
    InitObject(m_fullScreenQuad);
}

void FullScreenPass::CreateFramebuffer()
{
    HYP_SCOPE;

    if (m_flags & FSP_EXTERNAL_RENDERTARGET)
    {
        // will use RenderToFramebuffer() with other framebuffer one instead
        return;
    }

    AssertDebug(m_imageFormat != InvalidTextureFormat);

    if (m_framebuffer != nullptr)
    {
        // already created; check if size matches

        if (m_framebuffer->GetExtent() == m_extent)
        {
            // already set with correct extent, just create if not already
            CheckResult(m_framebuffer->Create());

            return;
        }

        EnqueueDeletion(std::move(m_framebuffer));
    }

    Assert(m_extent.Volume() != 0);

    Vec2u framebufferExtent = m_extent;

    if (ShouldRenderHalfRes())
    {
        static constexpr double ResolutionScale = 0.5;

        const uint32 numPixels = framebufferExtent.x * framebufferExtent.y;
        const int numPixelsScaled = MathUtil::Ceil(numPixels * ResolutionScale);

        const Vec2i reshapedExtent = MathUtil::ReshapeExtent(Vec2i { numPixelsScaled, 1 });

        // double the width as we swap between the two halves when rendering (checkerboarded)
        framebufferExtent = Vec2u { uint32(reshapedExtent.x * 2), uint32(reshapedExtent.y) };
    }

    RenderTargetDesc renderTargetDesc;
    renderTargetDesc.extent = framebufferExtent;
    renderTargetDesc.numLayers = 1;

    m_framebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);

    TextureDesc textureDesc;
    textureDesc.type = TextureType::Texture2D;
    textureDesc.format = m_imageFormat;
    textureDesc.extent = Vec3u { framebufferExtent, 1 };
    textureDesc.filterModeMin = TFM_NEAREST;
    textureDesc.filterModeMag = TFM_NEAREST;
    textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
    textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

    GpuImageRef attachmentImage = g_renderInterface->MakeImage(textureDesc);
    attachmentImage->SetDebugName(NAME_FMT("{}_RenderTargetTexture", InstanceClass()->GetName()));
    CheckResult(attachmentImage->Create());

    Attachment* attachment = m_framebuffer->AddAttachment(
        0,
        attachmentImage,
        ShouldRenderHalfRes() || (m_flags & FSP_RENDERTARGET_LOAD) ? LoadOperation::LOAD : LoadOperation::CLEAR,
        StoreOperation::STORE);

    CheckResult(attachment->Create());
    CheckResult(m_framebuffer->Create());
}

void FullScreenPass::CreateTemporalBlending()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending())
    {
        return;
    }

    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_extent,
        TextureFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        DefaultTemporalBlendingFeedback,
        ShouldRenderHalfRes()
            ? m_mergeHalfResTexturesPass->GetFinalImageView()
            : GetAttachment(0)->GetImageView(),
        m_gbuffer);

    m_temporalBlending->Create();
}

void FullScreenPass::CreateHistoryTexture()
{
    Assert(m_imageFormat != InvalidTextureFormat);

    if (m_historyTexture.IsValid())
    {
        EnqueueDeletion(std::move(m_historyTexture));
    }

    m_historyTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        m_imageFormat,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    });

    m_historyTexture->SetName(NAME_FMT("{}_FrameHistory", InstanceClass()->GetName()));

    InitObject(m_historyTexture);
}

void FullScreenPass::CreateMergeHalfResTexturesPass()
{
    HYP_SCOPE;

    if (!ShouldRenderHalfRes())
    {
        return;
    }

    MergeHalfResTexturesUniforms uniforms {};
    uniforms.dimensions = m_extent;

    if (!m_mergeHalfResTexturesUniformBuffer)
    {
        m_mergeHalfResTexturesUniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));
        CheckResult(m_mergeHalfResTexturesUniformBuffer->Create());
    }

    m_mergeHalfResTexturesUniformBuffer->Copy(sizeof(uniforms), &uniforms);

    m_mergeHalfResTexturesPass = MakeHandle<FullScreenPass>(
        ShaderDesc(NAME("MergeHalfResTextures")),
        m_imageFormat,
        m_extent,
        nullptr);

    m_mergeHalfResTexturesPass->Create();
}

void FullScreenPass::DrawHistoryTexture(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;
    
    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("BlitTexture");
    shaderDesc.properties.Add(s_propHalfRes);

    rq << SetCurrentShader(shaderDesc);

    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        rq << SetCurrentView(m_framebuffer->GetRenderTargetDesc(), Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        rq << SetCurrentView(m_framebuffer->GetRenderTargetDesc(), Viewport { m_framebuffer->GetExtent() });
    }

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);
    
    rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    rq << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(2, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(3, "InTexture"_sh, GetPreviousFrameColorImageView());

    RenderFullScreenQuad(frame, renderSetup);

    rq << SetDepthTest(true);
    rq << SetDepthWrite(true);
}

void FullScreenPass::CopyResultToPreviousTexture(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    Assert(m_historyTexture.IsValid());

    const GpuImageRef& srcImage = m_framebuffer->GetAttachment(0)->GetImage();
    const GpuImageRef& dstImage = m_historyTexture->GetGpuImage();

    rq << InsertBarrier(srcImage, RS_COPY_SRC);
    rq << InsertBarrier(dstImage, RS_COPY_DST);

    rq << Blit(srcImage, dstImage);

    rq << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
    rq << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
}

void FullScreenPass::MergeHalfResTextures(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    Assert(m_mergeHalfResTexturesPass != nullptr);

    m_mergeHalfResTexturesPass->Begin(frame, renderSetup);

    rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    rq << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(2, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(3, "InTexture"_sh, GetAttachment(0)->GetImageView());
    rq << SetShaderUniform(4, "UniformBuffer"_sh, m_mergeHalfResTexturesUniformBuffer);

    m_mergeHalfResTexturesPass->RenderFullScreenQuad(frame, renderSetup);

    m_mergeHalfResTexturesPass->End(frame, renderSetup);
}

void FullScreenPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Render() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderToFramebuffer(frame, renderSetup, m_framebuffer);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }
}

void FullScreenPass::RenderToFramebuffer(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);
    AssertDebug(framebuffer != nullptr);

    RenderQueue& rq = frame->renderQueue;

    // are we responsible for starting/ending framebuffer recording?
    bool shouldStartRecording = !framebuffer->IsDeferredRecording();
    bool shouldEndRecording = shouldStartRecording;

    Array<InsertBarrier, RenderAllocator> preRenderBarriers;

    /*if (!framebuffer->IsDeferredRecording())
    {
        for (int i = 0; i < framebuffer->NumAttachments(); i++)
        {
            AttachmentBase* attachment = framebuffer->GetAttachment(i);
            AssertDebug(attachment != nullptr);

            if (attachment->GetLoadOperation() == LoadOperation::LOAD)
            {
                preRenderBarriers.PushBack(InsertBarrier(attachment->GetImage(), attachment->IsDepthAttachment() ? RS_DEPTH_STENCIL : RS_RENDER_TARGET));
            }
        }
    }*/

    if (preRenderBarriers.Any())
    {
        for (InsertBarrier& ib : preRenderBarriers)
        {
            rq << ib;
        }
    }

    if (shouldStartRecording)
    {
        rq << BeginFramebuffer(framebuffer);
    }
    
    rq << SetCurrentShader(m_shaderDesc);

    RenderToFramebuffer_Internal(frame, renderSetup, framebuffer);

    if (shouldEndRecording)
    {
        rq << EndFramebuffer(framebuffer);
    }

    m_isFirstFrame = false;
}

void FullScreenPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    RenderQueue& rq = frame->renderQueue;

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_historyTexture.IsValid())
    {
        DrawHistoryTexture(frame, renderSetup);
    }

    if (ShouldRenderHalfRes())
    {
        Assert(framebuffer != nullptr, "Framebuffer must be set before rendering to it, if rendering at half res");

        const Vec2i viewportOffset = (Vec2i(framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(framebuffer->GetExtent().x / 2, framebuffer->GetExtent().y);

        rq << SetCurrentView(framebuffer->GetRenderTargetDesc(), Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        rq << SetCurrentView(framebuffer->GetRenderTargetDesc(), Viewport { framebuffer->GetExtent() });
    }

    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);
    
    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("BlitTexture");
    rq << SetCurrentShader(shaderDesc);

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);
    rq << SetFaceCullMode(FCM_BACK);
    rq << SetFillMode(FM_FILL);
    rq << SetTopology(TOP_TRIANGLES);
    rq << SetCurrentBlendFunction(m_blendFunction);

    RenderFullScreenQuad(frame, renderSetup);

    rq << SetDepthTest(true);
    rq << SetDepthWrite(true);
    rq << SetCurrentBlendFunction(BlendFunction::None());
}

void FullScreenPass::RenderFullScreenQuad(Frame* frame, const RenderSetup& renderSetup)
{
    frame->renderQueue << CommitDrawState();

    frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(6);
}

void FullScreenPass::Begin(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    RenderQueue& rq = frame->renderQueue;

    rq << BeginFramebuffer(m_framebuffer);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_historyTexture.IsValid())
    {
        DrawHistoryTexture(frame, renderSetup);
    }

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        rq << SetCurrentView(m_framebuffer->GetRenderTargetDesc(), Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        rq << SetCurrentView(m_framebuffer->GetRenderTargetDesc(), Viewport { m_framebuffer->GetExtent() });
    }

    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);
    
    rq << SetCurrentShader(m_shaderDesc);

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);
    rq << SetFaceCullMode(FCM_BACK);
    rq << SetFillMode(FM_FILL);
    rq << SetTopology(TOP_TRIANGLES);
    rq << SetCurrentBlendFunction(m_blendFunction);
}

void FullScreenPass::End(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    rq << SetDepthTest(true);
    rq << SetDepthWrite(true);
    rq << SetCurrentBlendFunction(BlendFunction::None());

    rq << EndFramebuffer(m_framebuffer);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isFirstFrame = false;
}

} // namespace Hyperion
