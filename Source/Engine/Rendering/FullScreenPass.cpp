/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/TemporalBlending.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Rendering/passes/DeferredPass.hpp>

#include <Rendering/util/DeletionQueue.hpp>
#include <Rendering/util/ShaderPropertyDictionary.hpp>

#include <Scene/View.hpp>

#include <Core/math/MathUtil.hpp>

#include <Rendering/util/MeshBuilder.hpp>

namespace Hyperion {

static const ShaderPropertyId s_propCheckerboarded = InternShaderProperty(ShaderProperty(NAME("CHECKERBOARDED")));

struct MergeCheckerboardUniforms
{
    Vec2u dimensions;
};

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
    m_threadSignal.Wait();

    m_fullScreenQuad.Reset();

    EnqueueDeletion(std::move(m_framebuffer));

    // not calling EnqueueDeletion() for graphics pipeline as it is managed by the graphics pipeline caching system
}

const GpuImageViewRef& FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending())
    {
        Assert(m_temporalBlending != nullptr);

        return RI.textureViewCache->GetOrCreate(m_temporalBlending->GetResultTexture());
    }

    if (ShouldRenderCheckerboarded())
    {
        return m_mergeCheckerboardPass->GetFinalImageView();
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
    if (ShouldRenderCheckerboarded())
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
        return RI.textureViewCache->GetOrCreate(m_historyTexture);
    }

    return GpuImageViewRef::Null();
}

void FullScreenPass::Create()
{
    Assert(!m_isInitialized);

    CreateFullScreenQuad();
    CreateFramebuffer();
    CreateMergeCheckerboardPass();
    CreateTemporalBlending();

    /// @NOTE: if we use temporal blending and we're not rendering at half res, we need a history texture,
    ///        so we blit to it each frame after rendering and use it as input the next frame.
    if (UsesTemporalBlending() && !ShouldRenderCheckerboarded())
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
    if (IsOnThread(g_renderThread))
    {
        Resize_Internal(newSize);
        return;
    }

    m_threadSignal.WaitAndReset();

    GetThreadById(g_renderThread)->GetScheduler().Enqueue([this, newSize]()
        {
            if (m_isInitialized)
            {
                Resize_Internal(newSize);
            }
            else
            {
                m_extent = newSize;
            }

            m_threadSignal.Signal();
        }, TaskEnqueueFlags::FIRE_AND_FORGET);
}

void FullScreenPass::Resize_Internal(Vec2u newSize)
{
    AssertOnThread(g_renderThread);

    if (m_extent == newSize)
    {
        return;
    }

    AssertDebug(newSize.Volume() != 0, "Cannot resize FullScreenPass to zero size!");

    newSize = MathUtil::Max(newSize, Vec2u::One());

    HYP_LOG(Rendering, Verbose, "Resizing FullScreenPass from {} to {}",
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

    CreateMergeCheckerboardPass();
    CreateTemporalBlending();

    if (UsesTemporalBlending() && !ShouldRenderCheckerboarded())
    {
        CreateHistoryTexture();
    }
}

void FullScreenPass::CreateFullScreenQuad()
{
    m_fullScreenQuad = MeshBuilder::Quad();
    m_fullScreenQuad->SetFlags(MeshFlags::ViewIndependent);
    m_fullScreenQuad->UploadGpuData();
}

void FullScreenPass::CreateFramebuffer()
{
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

    if (ShouldRenderCheckerboarded())
    {
        static constexpr double ResolutionScale = 0.5;

        const uint32 numPixels = framebufferExtent.x * framebufferExtent.y;
        const int numPixelsScaled = ByteUtil::AlignAs(MathUtil::Ceil(numPixels * ResolutionScale), 16);

        const Vec2i reshapedExtent = MathUtil::ReshapeExtent(Vec2i { numPixelsScaled, 1 });

        // double the width as we swap between the two halves when rendering (checkerboarded)
        framebufferExtent = Vec2u { uint32(reshapedExtent.x * 2), uint32(reshapedExtent.y) };
    }
    else if (ShouldRenderHalfRes() && m_extent.Volume() > 1)
    {
        framebufferExtent /= 2;
    }

    FramebufferDesc framebufferDesc;
    framebufferDesc.extent = framebufferExtent;
    framebufferDesc.numLayers = 1;

    m_framebuffer = RI.MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
    m_framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", GetName()));
#endif

    Attachment* attachment = m_framebuffer->AddAttachment(
        0,
        AttachmentDesc {
            TextureType::Texture2D,
            m_imageFormat,
            ShouldRenderCheckerboarded() ? LoadOperation::LOAD : LoadOperation::CLEAR,
            StoreOperation::STORE
        });

    CheckResult(attachment->Create());
    CheckResult(m_framebuffer->Create());
}

void FullScreenPass::CreateTemporalBlending()
{
    if (!UsesTemporalBlending())
    {
        return;
    }

    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_extent,
        TextureFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        DefaultTemporalBlendingFeedback,
        ShouldRenderCheckerboarded()
            ? m_mergeCheckerboardPass->GetFinalImageView()
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

    m_historyTexture->SetName(NAME_FMT("{}_FrameHistory", GetName()));

    CheckResult(m_historyTexture->Create());
}

void FullScreenPass::CreateMergeCheckerboardPass()
{
    if (!ShouldRenderCheckerboarded())
    {
        return;
    }

    MergeCheckerboardUniforms uniforms {};
    uniforms.dimensions = m_extent;

    if (!m_mergeCheckerboardUniformBuffer)
    {
        m_mergeCheckerboardUniformBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(uniforms));
        CheckResult(m_mergeCheckerboardUniformBuffer->Create());
    }

    m_mergeCheckerboardUniformBuffer->Copy(sizeof(uniforms), &uniforms);
    m_mergeCheckerboardUniformBuffer->Flush(0, sizeof(uniforms));

    m_mergeCheckerboardPass = MakeUnique<FullScreenPass>(
        ShaderDesc(NAME("MergeCheckerboard")),
        m_imageFormat,
        m_extent,
        nullptr);

    m_mergeCheckerboardPass->Create();
}

void FullScreenPass::DrawHistoryTexture(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);

    CommandRecorder& cr = frame->cr;

    ShaderDesc shaderDesc;
    shaderDesc.name = NAME("BlitTexture");
    shaderDesc.properties.Set(s_propCheckerboarded, ShouldRenderCheckerboarded());

    cr << SetCurrentShader(shaderDesc);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    if (ShouldRenderCheckerboarded())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        cr << SetCurrentViewport(Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        cr << SetCurrentViewport(Viewport { m_framebuffer->GetExtent() });
    }

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(2, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(3, "InTexture"_sh, GetPreviousFrameColorImageView());

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
}

void FullScreenPass::CopyResultToPreviousTexture(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    CommandRecorder& cr = frame->cr;

    Assert(m_historyTexture.IsValid());

    const GpuImageRef& srcImage = m_framebuffer->GetAttachment(0)->GetGpuImage();
    const GpuImageRef& dstImage = m_historyTexture->GetGpuImage();

    cr << InsertBarrier(srcImage, RS_COPY_SRC);
    cr << InsertBarrier(dstImage, RS_COPY_DST);

    cr << CopyImage(srcImage, dstImage, srcImage->GetTextureDesc().extent);

    cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
    cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
}

void FullScreenPass::MergeCheckerboard(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    CommandRecorder& cr = frame->cr;

    Assert(m_mergeCheckerboardPass != nullptr);

    m_mergeCheckerboardPass->Begin(frame, renderSetup);

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(2, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(3, "InTexture"_sh, GetAttachment(0)->GetImageView());
    cr << SetShaderUniform(4, "UniformBuffer"_sh, m_mergeCheckerboardUniformBuffer);

    m_mergeCheckerboardPass->RenderFullScreenQuad(frame, renderSetup);

    m_mergeCheckerboardPass->End(frame, renderSetup);
}

void FullScreenPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Render() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    RenderToFramebuffer(frame, renderSetup, m_framebuffer);

    if (ShouldRenderCheckerboarded())
    {
        MergeCheckerboard(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderCheckerboarded())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }
}

void FullScreenPass::RenderToFramebuffer(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world);
    AssertDebug(framebuffer != nullptr);

    RenderToFramebuffer_Internal(frame, renderSetup, framebuffer);

    m_isFirstFrame = false;
}

void FullScreenPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    AssertOnThread(g_renderThread);

    CommandRecorder& cr = frame->cr;

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_historyTexture.IsValid())
    {
        DrawHistoryTexture(frame, renderSetup);
    }

    if (ShouldRenderCheckerboarded())
    {
        Assert(framebuffer != nullptr, "Framebuffer must be set before rendering to it, if rendering at half res");

        const Vec2i viewportOffset = (Vec2i(framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(framebuffer->GetExtent().x / 2, framebuffer->GetExtent().y);

        cr << SetCurrentViewport(Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        cr << SetCurrentViewport(Viewport { framebuffer->GetExtent() });
    }

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetFaceCullMode(FCM_NONE); // FCM_BACK);
    cr << SetFillMode(FM_FILL);
    cr << SetTopology(TOP_TRIANGLES);
    cr << SetCurrentBlendFunction(m_blendFunction);

    RenderFullScreenQuad(frame, renderSetup);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());
}

void FullScreenPass::RenderFullScreenQuad(Frame* frame, const RenderSetup& renderSetup)
{
    frame->cr << CommitDrawState();

    frame->cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->cr << DrawIndexed(6);
}

void FullScreenPass::Begin(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentFramebuffer(m_framebuffer);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_historyTexture.IsValid())
    {
        DrawHistoryTexture(frame, renderSetup);
    }

    if (ShouldRenderCheckerboarded())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        cr << SetCurrentViewport(Viewport { viewportExtent, viewportOffset });
    }
    else
    {
        // render previous frame's result to screen
        cr << SetCurrentViewport(Viewport { m_framebuffer->GetExtent() });
    }

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetTopology(TOP_TRIANGLES);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetFaceCullMode(FCM_NONE);//FCM_BACK);
    cr << SetFillMode(FM_FILL);
    cr << SetCurrentBlendFunction(m_blendFunction);
}

void FullScreenPass::End(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    AssertDebug(!(m_flags & FSP_EXTERNAL_RENDERTARGET), "Cannot use Begin()/End() with external target, use RenderToFramebuffer() instead");
    AssertDebug(m_framebuffer != nullptr);

    CommandRecorder& cr = frame->cr;

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentFramebuffer(nullptr);

    if (ShouldRenderCheckerboarded())
    {
        MergeCheckerboard(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderCheckerboarded())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isFirstFrame = false;
}

} // namespace Hyperion
