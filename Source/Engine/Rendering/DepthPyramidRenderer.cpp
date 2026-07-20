/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Attachment.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Sampler.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/SamplerCache.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineDriver.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

struct DepthPyramidUniforms
{
    Vec2u mipDimensions;
    Vec2u prevMipDimensions;
    uint32 mipLevel;
};

DepthPyramidRenderer::DepthPyramidRenderer(GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_isRendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer()
{
    EnqueueDeletion(std::move(m_depthImageView));

    EnqueueDeletion(std::move(m_mipImageViews));
    EnqueueDeletion(std::move(m_mipUniformBuffers));
}

void DepthPyramidRenderer::Create()
{
    Assert(m_gbuffer != nullptr);

    const FramebufferRef& opaqueFramebuffer = m_gbuffer->GetPass(GBufferPass::Opaque).framebuffer;
    Assert(opaqueFramebuffer.IsValid());

    AttachmentBase* depthAttachment = opaqueFramebuffer->GetAttachment(GBufferTarget::Depth);
    Assert(depthAttachment != nullptr);

    m_depthImageView = depthAttachment->GetImageView();
    Assert(m_depthImageView.IsValid());

    const GpuImageRef& depthImage = m_depthImageView->GetImage();
    Assert(depthImage.IsValid());

    // create depth pyramid image
    m_hzbTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RG32F,   // store both min and maxes.
        depthImage->GetExtent(),
        TFM_NEAREST_MIPMAP,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE
    });

    m_hzbTexture->SetName(NAME("HZBTexture"));

    Check(m_hzbTexture->Create());

    const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();
    const Vec3u& depthPyramidExtent = m_hzbTexture->GetExtent();

    const uint32 numMipLevels = m_hzbTexture->GetTextureDesc().NumMips();

    m_mipImageViews.Clear();
    m_mipImageViews.Reserve(numMipLevels);

    m_mipUniformBuffers.Clear();
    m_mipUniformBuffers.Reserve(numMipLevels);

    uint32 mipWidth = imageExtent.x;
    uint32 mipHeight = imageExtent.y;

    for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
    {
        const uint32 prevMipWidth = mipWidth;
        const uint32 prevMipHeight = mipHeight;

        mipWidth = MathUtil::Max(1u, depthPyramidExtent.x >> (mipLevel));
        mipHeight = MathUtil::Max(1u, depthPyramidExtent.y >> (mipLevel));

        DepthPyramidUniforms uniforms;
        uniforms.mipDimensions = { mipWidth, mipHeight };
        uniforms.prevMipDimensions = { prevMipWidth, prevMipHeight };
        uniforms.mipLevel = mipLevel;

        GpuBufferRef& mipUniformBuffer = m_mipUniformBuffers.PushBack(RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(DepthPyramidUniforms)));
#ifdef HYP_RHI_DEBUG_NAMES
        mipUniformBuffer->SetDebugName(NAME_FMT("DepthPyramid_Mip{}_UniformBuffer", mipLevel));
#endif
        Check(mipUniformBuffer->Create());

        mipUniformBuffer->Copy(sizeof(DepthPyramidUniforms), &uniforms);
        mipUniformBuffer->Flush(0, sizeof(DepthPyramidUniforms));

        GpuImageViewRef& mipImageView = m_mipImageViews.PushBack(RI.MakeImageView(m_hzbTexture->GetGpuImage(), mipLevel, 1, 0, 1));
#ifdef HYP_RHI_DEBUG_NAMES
        mipImageView->SetDebugName(NAME_FMT("DepthPyramid_Mip{}_ImageView", mipLevel));
#endif

        Check(mipImageView->Create());
    }
}

Vec2u DepthPyramidRenderer::GetExtent() const
{
    if (!m_hzbTexture.IsValid())
    {
        return Vec2u::One();
    }

    const Vec3u& extent = m_hzbTexture->GetExtent();

    return { extent.x, extent.y };
}

void DepthPyramidRenderer::Render(Frame* frame)
{
    Sampler* depthPyramidSampler = RI.samplerCache->GetOrCreate(SamplerDesc { TFM_NEAREST_MIPMAP, TFM_NEAREST, TWM_CLAMP_TO_EDGE });

    const uint8 numDepthPyramidMipLevels = uint8(m_mipImageViews.Size());

    const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();

    const Vec3u& depthPyramidExtent = m_hzbTexture->GetExtent();

    uint32 mipWidth = imageExtent.x;
    uint32 mipHeight = imageExtent.y;

    for (uint8 mipLevel = 0; mipLevel < numDepthPyramidMipLevels; mipLevel++)
    {
        const uint32 prevMipWidth = mipWidth;
        const uint32 prevMipHeight = mipHeight;

        mipWidth = MathUtil::Max(1u, depthPyramidExtent.x >> (mipLevel));
        mipHeight = MathUtil::Max(1u, depthPyramidExtent.y >> (mipLevel));

        // Set the compute shader
        frame->cr << SetCurrentShader(ShaderDesc(NAME("GenerateDepthPyramid")));

        if (mipLevel == 0)
        {
            // first mip level -- input is the actual depth image
            frame->cr << SetShaderUniform(0, "InImage"_sh, m_depthImageView);
        }
        else
        {
            frame->cr << SetShaderUniform(0, "InImage"_sh, m_mipImageViews[mipLevel - 1]);
        }

        frame->cr << SetShaderUniform(1, "OutImage"_sh, m_mipImageViews[mipLevel]);
        frame->cr << SetShaderUniform(2, "CBuffer"_sh, m_mipUniformBuffers[mipLevel]);
        frame->cr << SetShaderUniform(3, "DepthPyramidSampler"_sh, depthPyramidSampler);

        frame->cr << DispatchCompute({ (mipWidth + 7) / 8, (mipHeight + 7) / 8, 1 });

        frame->cr << InsertUAVBarrier(m_hzbTexture->GetGpuImage());
    }

    frame->cr << InsertBarrier(m_hzbTexture->GetGpuImage(), RS_SHADER_RESOURCE);

    m_isRendered = true;
}

} // namespace Hyperion
