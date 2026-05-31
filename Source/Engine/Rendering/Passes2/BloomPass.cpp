/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/passes/BloomPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/passes/DeferredPass.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <Rendering/CBufferAllocator.hpp>

#include <Scene/View.hpp>

#include <System/AppContext.hpp>

#include <Core/math/Vector2.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <Asset/AssetRegistry.hpp>

namespace Hyperion {

class DeferredPassData;

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statBloom("Rendering/GPU/Bloom");

struct BloomUniforms
{
    Vec2u dimension;
    float threshold;
    float intensity;
    float softKnee;
};

struct DownsampleUniforms
{
    Vec2u srcDimension;
    Vec2u dstDimension;
    Vec2f invDimension;
    Vec2f padding;
};

struct UpsampleUniforms
{
    Vec2f texelSize;
    float scatter; // blend factor: 0=only coarse mips, 1=only fine mips (0.5 is a good default)
    float padding;
};

CVar<float> cvBloomThreshold { "Rendering.BloomThreshold", 1.0f, "Rendering.Bloom.Threshold" };
CVar<float> cvBloomIntensity { "Rendering.BloomIntensity", 0.4f, "Rendering.Bloom.Intensity" };
CVar<float> cvBloomSoftKnee { "Rendering.BloomSoftKnee", 0.5f, "Rendering.Bloom.SoftKnee" };

BloomPass::BloomPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_samplerClampToEdge(RI.samplerCache->GetOrCreate(SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_CLAMP_TO_EDGE }))
{
    SetPassName(NAME("Bloom"));
}

BloomPass::~BloomPass() = default;

//BloomPass::~BloomPass()
//{
//    m_bloomResult.Reset();
//
//    EnqueueDeletion(FunctionWrapper<Proc<void()>>([
//        brightExtractTexture = m_brightExtractTexture,
//        downsamplePasses = std::move(m_downsamplePasses),
//        upsamplePasses = std::move(m_upsamplePasses)]() mutable -> void
//    {
//        brightExtractTexture.Reset();
//
//        for (size_t i = 0; i < downsamplePasses.Size(); i++)
//        {
//            downsamplePasses[i].Reset();
//        }
//
//        for (size_t i = 0; i < upsamplePasses.Size(); i++)
//        {
//            upsamplePasses[i].Reset();
//        }
//    }));
//}

void BloomPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    // Updates m_extent and recreates the base framebuffer.
    FullScreenPass::Resize_Internal(newSize);

    const Vec2u extent = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;

    // Recreate the bright-extract texture at the new resolution.
    m_brightExtractTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA16F,
        Vec3u(extent, 1),
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });
    m_brightExtractTexture->SetIsTransient(true);
    m_brightExtractTexture->SetName(NAME("BloomBrightExtract"));
    CheckResult(m_brightExtractTexture->Create());
    GetEngineAssetRegistry()->PutAsset(m_brightExtractTexture);

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        const Vec2u targetExtent = MathUtil::Max(Vec2u(extent.x >> (i + 1), extent.y >> (i + 1)), Vec2u::One());
        m_downsamplePasses[i]->Resize(targetExtent);
    }

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        const Vec2u targetExtent = MathUtil::Max(Vec2u(extent.x >> (NumMipLevels - 2 - i), extent.y >> (NumMipLevels - 2 - i)), Vec2u::One());
        m_upsamplePasses[i]->Resize(targetExtent);
    }

    m_bloomResult = MakeStrongRef(m_upsamplePasses[NumMipLevels - 2]->GetAttachment(0));
}

void BloomPass::Create()
{
    Assert(m_gbuffer != nullptr);

    FullScreenPass::Create();

    Vec2u extent = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;

    m_brightExtractTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA16F,
        Vec3u(extent, 1),
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED
    });
    m_brightExtractTexture->SetIsTransient(true);
    m_brightExtractTexture->SetName(NAME("BloomBrightExtract"));
    CheckResult(m_brightExtractTexture->Create());

    GetEngineAssetRegistry()->PutAsset(m_brightExtractTexture);

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        // Each downsample step halves resolution: ds[0]=1/2, ds[1]=1/4, ds[2]=1/8 ...
        Vec2u targetExtent = MathUtil::Max(Vec2u(extent.x >> (i + 1), extent.y >> (i + 1)), Vec2u::One());

        m_downsamplePasses[i] = MakeUnique<FullScreenPass>(
            TextureFormat::RGBA16F,
            targetExtent,
            nullptr,
            FSP_NONE);

        m_downsamplePasses[i]->SetShaderDesc(ShaderDesc(NAME("BloomDownsample"), GetShaderProperties()));
        m_downsamplePasses[i]->Create();
    }

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        Vec2u targetExtent = MathUtil::Max(Vec2u(extent.x >> (NumMipLevels - 2 - i), extent.y >> (NumMipLevels - 2 - i)), Vec2u::One());

        m_upsamplePasses[i] = MakeUnique<FullScreenPass>(
            TextureFormat::RGBA16F,
            targetExtent,
            nullptr,
            FSP_NONE);

        m_upsamplePasses[i]->SetShaderDesc(ShaderDesc(NAME("BloomUpsample"), GetShaderProperties()));
        m_upsamplePasses[i]->Create();
    }
}

void BloomPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    ENGINE_STAT_GPU_SCOPE(&s_statBloom);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);

    CommandRecorder& cr = frame->cr;

    ExtractBrightAreas(frame, renderSetup, inputsFramebuffer, dpd);
    Downsample(frame, renderSetup);
    Upsample(frame, renderSetup);
}

ShaderPropertySet BloomPass::GetShaderProperties() const
{
    ShaderPropertySet shaderProperties;
    return shaderProperties;
}

void BloomPass::ExtractBrightAreas(Frame* frame, const RenderSetup& renderSetup, const FramebufferRef& inputsFramebuffer, DeferredPassData* dpd)
{
    CommandRecorder& cr = frame->cr;

    const Vec2u extent = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;

    const Vec2u numGroups(
        (extent.x + 15) / 16,
        (extent.y + 15) / 16);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    BloomUniforms bloomConstants {};
    bloomConstants.dimension = extent;
    bloomConstants.threshold = cvBloomThreshold.Get();
    bloomConstants.intensity = cvBloomIntensity.Get();
    bloomConstants.softKnee = cvBloomSoftKnee.Get();

    RI.cbufferAllocator->Write(&bloomConstants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_brightExtractTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    cr << SetCurrentShader(ShaderDesc(NAME("BloomExtract"), GetShaderProperties()));

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "OutImage"_sh, RI.textureViewCache->GetOrCreate(m_brightExtractTexture));
    cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << SetShaderUniform(numShaderUniforms++, "DeferredShadingTexture"_sh, dpd->deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, m_samplerClampToEdge);
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    cr << DispatchCompute(Vec3u { numGroups.x, numGroups.y, 1 });

    cr << InsertBarrier(m_brightExtractTexture->GetGpuImage(), RS_SHADER_RESOURCE);
}

void BloomPass::Downsample(Frame* frame, const RenderSetup& renderSetup)
{
    CommandRecorder& cr = frame->cr;

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        FullScreenPass* pass = m_downsamplePasses[i].Get();

        Texture* srcTexture = (i == 0) ? m_brightExtractTexture.Get() : m_downsamplePasses[i - 1]->GetAttachment(0);

        const Vec2u srcExtent = (i == 0) ? m_extent : m_downsamplePasses[i - 1]->GetExtent();
        const Vec2u dstExtent = pass->GetExtent();

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        DownsampleUniforms constants {};
        constants.srcDimension = srcExtent;
        constants.dstDimension = dstExtent;
        constants.invDimension = Vec2f(1.0f) / Vec2f(dstExtent);

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        pass->Begin(frame, renderSetup);

        uint32 numShaderUniforms = 0;

        cr << SetShaderUniform(numShaderUniforms++, "InputTexture"_sh, RI.textureViewCache->GetOrCreate(srcTexture));
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, m_samplerClampToEdge);
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        pass->RenderFullScreenQuad(frame, renderSetup);

        pass->End(frame, renderSetup);

        cr << InsertBarrier(pass->GetAttachment(0)->GetGpuImage(), RS_SHADER_RESOURCE);
    }
}

void BloomPass::Upsample(Frame* frame, const RenderSetup& renderSetup)
{
    CommandRecorder& cr = frame->cr;

    for (uint32 i = 0; i < NumMipLevels - 1; i++)
    {
        FullScreenPass* pass = m_upsamplePasses[i].Get();

        Texture* prevTexture = (i == 0)
            ? m_downsamplePasses[NumMipLevels - 2]->GetAttachment(0)
            : m_upsamplePasses[i - 1]->GetAttachment(0);

        Texture* blendTexture;
        if (i == NumMipLevels - 2)
        {
            blendTexture = m_brightExtractTexture.Get();
        }
        else
        {
            const uint32 dsBlendIndex = (NumMipLevels - 3) - i;
            blendTexture = m_downsamplePasses[dsBlendIndex]->GetAttachment(0);
        }

        const Vec2u targetExtent = m_upsamplePasses[i]->GetExtent();

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        UpsampleUniforms constants {};
        constants.texelSize = Vec2f(1.0f) / Vec2f(targetExtent);
        constants.scatter = 0.5f;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        pass->Begin(frame, renderSetup);

        uint32 numShaderUniforms = 0;

        cr << SetShaderUniform(numShaderUniforms++, "PrevPassTexture"_sh, RI.textureViewCache->GetOrCreate(prevTexture));
        cr << SetShaderUniform(numShaderUniforms++, "InputTexture"_sh, RI.textureViewCache->GetOrCreate(blendTexture));
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, m_samplerClampToEdge);
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        pass->RenderFullScreenQuad(frame, renderSetup);

        pass->End(frame, renderSetup);

        cr << InsertBarrier(pass->GetAttachment(0)->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    m_bloomResult = MakeStrongRef(m_upsamplePasses[NumMipLevels - 2]->GetAttachment(0));
}

const Handle<Texture>& BloomPass::GetBloomResult() const
{
    return m_bloomResult;
}

} // namespace Hyperion
