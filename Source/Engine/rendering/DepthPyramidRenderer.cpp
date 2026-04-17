/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Attachment.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/Sampler.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/SamplerCache.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <engine/EngineDriver.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

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

    EnqueueDeletion(std::move(m_depthPyramid));
    EnqueueDeletion(std::move(m_depthPyramidView));

    EnqueueDeletion(std::move(m_mipImageViews));
    EnqueueDeletion(std::move(m_mipUniformBuffers));
}

void DepthPyramidRenderer::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(m_gbuffer != nullptr);

    const FramebufferRef& opaqueFramebuffer = m_gbuffer->GetBucket(RenderBucket::Opaque).GetFramebuffer();
    Assert(opaqueFramebuffer.IsValid());

    AttachmentBase* depthAttachment = opaqueFramebuffer->GetAttachment(GTN_DEPTH);
    Assert(depthAttachment != nullptr);

    m_depthImageView = depthAttachment->GetImageView();
    Assert(m_depthImageView.IsValid());

    const GpuImageRef& depthImage = m_depthImageView->GetImage();
    Assert(depthImage.IsValid());

    // create depth pyramid image
    m_depthPyramid = g_renderInterface->MakeImage(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::R32F,
        Vec3u {
            MathUtil::Max(uint32(MathUtil::PreviousPowerOf2(depthImage->GetExtent().x + 1)), 1),
            MathUtil::Max(uint32(MathUtil::PreviousPowerOf2(depthImage->GetExtent().y + 1)), 1),
            1
        },
        TFM_NEAREST_MIPMAP,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE
    });

#if HYP_DEBUG_MODE
    m_depthPyramid->SetDebugName(NAME("DepthPyramid"));
#endif

    m_depthPyramid->Create();

    m_depthPyramidView = g_renderInterface->MakeImageView(m_depthPyramid);
    m_depthPyramidView->Create();

    const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();
    const Vec3u& depthPyramidExtent = m_depthPyramid->GetExtent();

    const uint32 numMipLevels = m_depthPyramid->NumMips();

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

        GpuBufferRef& mipUniformBuffer = m_mipUniformBuffers.PushBack(g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(DepthPyramidUniforms)));
#if HYP_DEBUG_MODE
        mipUniformBuffer->SetDebugName(NAME_FMT("DepthPyramid_Mip{}_UniformBuffer", mipLevel));
#endif
        CheckResult(mipUniformBuffer->Create());

        mipUniformBuffer->Copy(sizeof(DepthPyramidUniforms), &uniforms);
        mipUniformBuffer->Flush(0, sizeof(DepthPyramidUniforms));

        GpuImageViewRef& mipImageView = m_mipImageViews.PushBack(g_renderInterface->MakeImageView(m_depthPyramid, mipLevel, 1, 0, m_depthPyramid->NumArrayLayers()));
#if HYP_DEBUG_MODE
        mipImageView->SetDebugName(NAME_FMT("DepthPyramid_Mip{}_ImageView", mipLevel));
#endif

        CheckResult(mipImageView->Create());
    }
}

Vec2u DepthPyramidRenderer::GetExtent() const
{
    if (!m_depthPyramid.IsValid())
    {
        return Vec2u::One();
    }

    const Vec3u extent = m_depthPyramid->GetExtent();

    return { extent.x, extent.y };
}

void DepthPyramidRenderer::Render(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Sampler* depthPyramidSampler = g_renderInterface->samplerCache->GetOrCreate(SamplerDesc { TFM_NEAREST_MIPMAP, TFM_NEAREST, TWM_CLAMP_TO_EDGE });

    const uint8 numDepthPyramidMipLevels = uint8(m_mipImageViews.Size());

    const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();
    const Vec3u& depthPyramidExtent = m_depthPyramid->GetExtent();

    uint32 mipWidth = imageExtent.x;
    uint32 mipHeight = imageExtent.y;

    for (uint8 mipLevel = 0; mipLevel < numDepthPyramidMipLevels; mipLevel++)
    {
        // level 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        /*frame->cr << InsertBarrier(
            m_depthPyramid,
            RS_UNORDERED_ACCESS,
            ImageSubResource {
                .baseMipLevel = mipLevel,
                .numLevels = 1,
                .baseArrayLayer = 0,
                .numLayers = 1
            });*/

        //if (mipLevel != 0)
        //{
        //    // put prev mip into readable state
        //    frame->cr << InsertBarrier(
        //        m_depthPyramid,
        //        RS_SHADER_RESOURCE,
        //        ImageSubResource {
        //            .baseMipLevel = uint8(mipLevel - 1),
        //            .numLevels = 1,
        //            .baseArrayLayer = 0,
        //            .numLayers = 1
        //        });
        //}

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
        frame->cr << SetShaderUniform(2, "UniformBuffer"_sh, m_mipUniformBuffers[mipLevel]);
        frame->cr << SetShaderUniform(3, "DepthPyramidSampler"_sh, depthPyramidSampler);
        
        frame->cr << DispatchCompute({ (mipWidth + 31) / 32, (mipHeight + 31) / 32, 1 });
    }

    frame->cr << InsertBarrier(
        m_depthPyramid, RS_SHADER_RESOURCE);

    m_isRendered = true;
}

} // namespace Hyperion
