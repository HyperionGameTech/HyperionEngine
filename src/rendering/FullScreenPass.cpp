/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderMemory.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

#include <core/reflection/Class.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

#include <util/MeshBuilder.hpp>

#include <FullScreenPass.generated.inl>

namespace hyperion {

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
    : FullScreenPass(TF_NONE, nullptr, flags)
{
}

FullScreenPass::FullScreenPass(TextureFormat imageFormat, GBuffer* gbuffer, EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(nullptr, imageFormat, Vec2u::Zero(), gbuffer, flags)
{
}

FullScreenPass::FullScreenPass(TextureFormat imageFormat, Vec2u extent, GBuffer* gbuffer, EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(nullptr, imageFormat, extent, gbuffer, flags)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer,
    EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(
          shader,
          descriptorTable,
          FramebufferRef::Null(),
          imageFormat,
          extent,
          gbuffer,
          flags)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer,
    EnumFlags<FullScreenPassFlags> flags)
    : FullScreenPass(
          shader,
          DescriptorTableRef::Null(),
          FramebufferRef::Null(),
          imageFormat,
          extent,
          gbuffer,
          flags)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    const FramebufferRef& framebuffer,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer,
    EnumFlags<FullScreenPassFlags> flags)
    : m_shader(shader),
      m_framebuffer(framebuffer),
      m_imageFormat(imageFormat),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_flags(flags),
      m_blendFunction(BlendFunction::None()),
      m_renderTargetType(RTT_SHADER_RESOURCE),
      m_isInitialized(false),
      m_isFirstFrame(true)
{
    if (descriptorTable.IsValid())
    {
        m_descriptorTable.Set(descriptorTable);
    }
}

FullScreenPass::~FullScreenPass()
{
    m_fullScreenQuad.Reset();

    SafeDelete(std::move(m_framebuffer));

    // not calling SafeDelete() for graphics pipeline as it is managed by the graphics pipeline caching system
}

GpuImageViewRef FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending())
    {
        Assert(m_temporalBlending != nullptr);

        return g_renderBackend->GetTextureImageView(m_temporalBlending->GetResultTexture());
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

GpuImageViewRef FullScreenPass::GetPreviousFrameColorImageView() const
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

    if (m_previousTexture.IsValid())
    {
        return g_renderBackend->GetTextureImageView(m_previousTexture);
    }

    return GpuImageViewRef::Null();
}

void FullScreenPass::Create()
{
    HYP_SCOPE;

    Assert(!m_isInitialized);

    CreateQuad();
    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();

    m_isInitialized = true;
}

void FullScreenPass::SetShader(const ShaderRef& shader)
{
    if (m_shader == shader)
    {
        return;
    }

    m_shader = shader;
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

    // throw away graphics pipeline cache handle to force recreation.
    m_graphicsPipelineCacheHandle = GraphicsPipelineCacheHandle();
}

void FullScreenPass::SetRenderTargetType(RenderTargetType renderTargetType)
{
    if (m_renderTargetType == renderTargetType)
    {
        return;
    }

    m_renderTargetType = renderTargetType;

    // throw away graphics pipeline cache handle to force recreation.
    m_graphicsPipelineCacheHandle = GraphicsPipelineCacheHandle();
}

const GraphicsPipelineRef& FullScreenPass::GetGraphicsPipeline()
{
    HYP_SCOPE;

    if (HYP_LIKELY(m_graphicsPipelineCacheHandle.IsAlive()))
    {
        return *m_graphicsPipelineCacheHandle;
    }

    CreatePipeline();

    AssertDebug(m_graphicsPipelineCacheHandle.IsAlive());

    return *m_graphicsPipelineCacheHandle;
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

    if (!m_framebuffer.IsValid())
    {
        // Not created yet; skip
        return;
    }

    // throw away graphics pipeline cache handle to force recreation.
    m_graphicsPipelineCacheHandle = GraphicsPipelineCacheHandle();

    SafeDelete(std::move(m_framebuffer));

    m_temporalBlending.Reset();

    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();
}

void FullScreenPass::CreateQuad()
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

    AssertDebug(m_imageFormat != TF_NONE);

    if (m_framebuffer != nullptr)
    {
        // already created; check if size matches

        if (m_framebuffer->GetExtent() == m_extent)
        {
            // already created with correct extent
            DeferCreate(m_framebuffer);

            return;
        }

        SafeDelete(std::move(m_framebuffer));
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

    m_framebuffer = g_renderBackend->MakeFramebuffer(framebufferExtent, m_renderTargetType);

    TextureDesc textureDesc;
    textureDesc.type = TT_TEX2D;
    textureDesc.format = m_imageFormat;
    textureDesc.extent = Vec3u { framebufferExtent, 1 };
    textureDesc.filterModeMin = TFM_NEAREST;
    textureDesc.filterModeMag = TFM_NEAREST;
    textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
    textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

    GpuImageRef attachmentImage = g_renderBackend->MakeImage(textureDesc);
    attachmentImage->SetDebugName(NAME_FMT("{}_RenderTargetTexture", InstanceClass()->GetName()));
    DeferCreate(attachmentImage);

    AttachmentRef attachment = m_framebuffer->AddAttachment(
        0,
        attachmentImage,
        ShouldRenderHalfRes() || (m_flags & FSP_RENDERTARGET_LOAD) ? LoadOperation::LOAD : LoadOperation::CLEAR,
        StoreOperation::STORE);

    DeferCreate(attachment);

    DeferCreate(m_framebuffer);
}

void FullScreenPass::CreatePipeline()
{
    HYP_SCOPE;

    const MeshAttributes meshAttributes {
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    };

    const MaterialAttributes materialAttributes {
        .fillMode = FM_FILL,
        .blendFunction = m_blendFunction,
        .flags = MAF_NONE
    };

    CreatePipeline(RenderableAttributeSet(meshAttributes, materialAttributes));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;

    Assert(m_shader != nullptr);
    Assert(m_framebuffer != nullptr);

    m_graphicsPipelineCacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        m_descriptorTable.GetOr(DescriptorTableRef::Null()),
        { &m_framebuffer, 1 },
        renderableAttributes);

    Assert(m_graphicsPipelineCacheHandle.IsAlive());
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
        TF_RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        ShouldRenderHalfRes()
            ? m_mergeHalfResTexturesPass->GetFinalImageView()
            : GetAttachment(0)->GetImageView(),
        m_gbuffer);

    m_temporalBlending->Create();
}

void FullScreenPass::CreatePreviousTexture()
{
    Assert(m_imageFormat != TF_NONE);

    // Create previous image
    m_previousTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE });
    m_previousTexture->SetName(NAME_FMT("{}_PreviousFrameTexture", InstanceClass()->GetName()));

    InitObject(m_previousTexture);
}

void FullScreenPass::CreateRenderTextureToScreenPass()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending())
    {
        return;
    }

    if (!ShouldRenderHalfRes())
    {
        CreatePreviousTexture();
    }

    // Create render texture to screen pass.
    // this is used to render the previous frame's result to the screen,
    // so we can blend it with the current frame's result (checkerboarded)

    ShaderProperties shaderProperties;

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Set(ShaderProperty(NAME("HALFRES")));
    }

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"), shaderProperties);
    Assert(renderTextureToScreenShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InTexture", GetPreviousFrameColorImageView());
    }

    DeferCreate(descriptorTable);

    m_renderTextureToScreenPass = CreateObject<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        nullptr);

    m_renderTextureToScreenPass->Create();
}

void FullScreenPass::CreateMergeHalfResTexturesPass()
{
    HYP_SCOPE;

    if (!ShouldRenderHalfRes())
    {
        return;
    }

    GpuBufferRef mergeHalfResTexturesUniformBuffer;

    { // Create uniform buffer
        MergeHalfResTexturesUniforms uniforms;
        uniforms.dimensions = m_extent;

        mergeHalfResTexturesUniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
        HYP_GFX_ASSERT(mergeHalfResTexturesUniformBuffer->Create());
        mergeHalfResTexturesUniformBuffer->Copy(sizeof(uniforms), &uniforms);
    }

    ShaderRef mergeHalfResTexturesShader = g_shaderManager->GetOrCreate(NAME("MergeHalfResTextures"));
    Assert(mergeHalfResTexturesShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        mergeHalfResTexturesShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("MergeHalfResTexturesDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InTexture", GetAttachment(0)->GetImageView());
        descriptorSet->SetElement("UniformBuffer", mergeHalfResTexturesUniformBuffer);
    }

    DeferCreate(descriptorTable);

    m_mergeHalfResTexturesPass = CreateObject<FullScreenPass>(
        mergeHalfResTexturesShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        nullptr);

    m_mergeHalfResTexturesPass->Create();
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::RenderPreviousTextureToScreen(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_renderTextureToScreenPass != nullptr);

    const GraphicsPipelineRef& graphicsPipeline = m_renderTextureToScreenPass->GetGraphicsPipeline();

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi::GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        // render previous frame's result to screen
        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { m_framebuffer->GetExtent() });
    }

    frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(m_fullScreenQuad->NumIndices());
}

void FullScreenPass::CopyResultToPreviousTexture(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_previousTexture.IsValid());

    const GpuImageRef& srcImage = m_framebuffer->GetAttachment(0)->GetImage();
    const GpuImageRef& dstImage = m_previousTexture->GetGpuImage();

    frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);
    frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);

    frame->renderQueue << Blit(srcImage, dstImage);

    frame->renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
    frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
}

void FullScreenPass::MergeHalfResTextures(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_mergeHalfResTexturesPass != nullptr);

    m_mergeHalfResTexturesPass->Render(frame, renderSetup);
}

void FullScreenPass::Render(FrameBase* frame, const RenderSetup& renderSetup)
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

void FullScreenPass::RenderToFramebuffer(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);
    
    AssertDebug(framebuffer != nullptr);

    // are we responsible for starting/ending framebuffer recording?
    bool shouldStartRecording = !framebuffer->IsDeferredRecording();
    bool shouldEndRecording = shouldStartRecording;

    Array<InsertBarrier, RenderTempAllocator> preRenderBarriers;
    Array<InsertBarrier, RenderTempAllocator> postRenderBarriers;

    // we need to insert a barrier if any attachments are LOAD operations
    for (int i = 0; i < framebuffer->NumAttachments(); i++)
    {
        AttachmentBase* attachment = framebuffer->GetAttachment(i);
        AssertDebug(attachment != nullptr);

        if (attachment->GetLoadOperation() == LoadOperation::LOAD)
        {
            preRenderBarriers.PushBack(InsertBarrier(attachment->GetImage(), attachment->IsDepthAttachment() ? RS_DEPTH_STENCIL : RS_RENDER_TARGET));
        }
    }

    if (preRenderBarriers.Any())
    {
        if (framebuffer->IsDeferredRecording())
        {
            // if we need to insert barriers we need to do it outside of the pass
            frame->renderQueue << EndFramebuffer(framebuffer);

            shouldStartRecording = true; // we need to start new recording but should preserve the state (it was already recording)
        }

        for (const InsertBarrier& cmd : preRenderBarriers)
        {
            frame->renderQueue << cmd;
        }
    }

    if (shouldStartRecording)
    {
        frame->renderQueue << BeginFramebuffer(framebuffer);
    }

    RenderToFramebuffer_Internal(frame, renderSetup, framebuffer);

    if (shouldEndRecording)
    {
        frame->renderQueue << EndFramebuffer(framebuffer);
    }

    if (postRenderBarriers.Any())
    {
        for (const InsertBarrier& cmd : postRenderBarriers)
        {
            frame->renderQueue << cmd;
        }
    }

    m_isFirstFrame = false;
}

void FullScreenPass::RenderToFramebuffer_Internal(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, renderSetup);
    }

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline();

    graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

    if (ShouldRenderHalfRes())
    {
        Assert(framebuffer != nullptr, "Framebuffer must be set before rendering to it, if rendering at half res");

        const Vec2i viewportOffset = (Vec2i(framebuffer->GetExtent().x, 0) / 2) * (RenderApi::GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(framebuffer->GetExtent().x / 2, framebuffer->GetExtent().y);

        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        if (renderSetup.view != nullptr)
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, renderSetup.view->GetViewport());
        }
        else
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { framebuffer->GetExtent() });
        }
    }

    if (renderSetup.view != nullptr && renderSetup.view->GetCamera() != nullptr)
    {
        frame->renderQueue << BindDescriptorTable(
            graphicsPipeline->GetDescriptorTable(),
            graphicsPipeline,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frame->GetFrameIndex());
    }

    const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

    if (viewDescriptorSetIndex != ~0u)
    {
        AssertDebug(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            graphicsPipeline,
            {},
            viewDescriptorSetIndex);
    }

    Render_Internal(frame, renderSetup, graphicsPipeline);

    frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(6);
}

void FullScreenPass::Begin(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline();

    frame->renderQueue << BeginFramebuffer(m_framebuffer);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, renderSetup);
    }

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi::GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, renderSetup.view->GetViewport());
    }

    Render_Internal(frame, renderSetup, graphicsPipeline);
}

void FullScreenPass::End(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    frame->renderQueue << EndFramebuffer(m_framebuffer);

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

} // namespace hyperion
