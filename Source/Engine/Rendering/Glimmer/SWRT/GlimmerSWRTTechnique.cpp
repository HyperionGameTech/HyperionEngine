/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTTechnique.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Frame.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSWRTDebug("Rendering/GPU/Glimmer/SWRTDebug");

static EngineStatCounter<uint32> s_statGlimmerSWRTDebugRays("Rendering/Glimmer/SWRTDebugRays");

// the region follows the viewer in steps, so the TLAS only rebuilds when it has moved this fraction of the SWRT radius
static constexpr float RegionRecenterFraction = 0.25f;

// Must match GlimmerSWRTDebugConstants in Shaders/Glimmer/SWRT/GlimmerSWRTDebug.hlsl
struct GlimmerSWRTDebugConstants
{
    Vec4u dimensionsModeInstances;
    GlimmerFootprintMaskShaderData mask;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
};

static float GetNearFieldRadius()
{
    return MathUtil::Max(g_cvGlimmerSWRTNearFieldRadius.Get(), 8.0f);
}

GlimmerSceneRegionParams GlimmerSWRTTechnique::GetSceneRegionParams()
{
    const float nearFieldRadius = GetNearFieldRadius();

    // SWRT only covers the middle of the region; the wider span region rides along with it
    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(nearFieldRadius, g_cvGlimmerSWRTSpansRadius.Get());
    params.recenterDistance = nearFieldRadius * RegionRecenterFraction;
    params.snap = GlimmerFootprintMaskCellSize;

    return params;
}

// SWRT only covers the middle of the scene region; the rest is only splatted into the heightfield
static float GetSWRTRadius(const BoundingBox& sceneRegion)
{
    return MathUtil::Min(GetNearFieldRadius(), 0.5f * sceneRegion.GetExtent().x);
}

GlimmerSWRTTechnique::GlimmerSWRTTechnique()
    : m_footprintMask(MakeUnique<GlimmerFootprintMask>()),
      m_probeVolume(MakeUnique<GlimmerSWRTProbeVolume>()),
      m_maskGeneration(~0u)
{
}

GlimmerSWRTTechnique::~GlimmerSWRTTechnique()
{
}

BoundingBox GlimmerSWRTTechnique::GetTracedRegion(const BoundingBox& sceneRegion) const
{
    const Vec3f regionCenter = sceneRegion.GetCenter();
    const float swrtRadius = GetSWRTRadius(sceneRegion);

    BoundingBox swrtRegion = sceneRegion;
    swrtRegion.min.x = regionCenter.x - swrtRadius;
    swrtRegion.max.x = regionCenter.x + swrtRadius;
    swrtRegion.min.z = regionCenter.z - swrtRadius;
    swrtRegion.max.z = regionCenter.z + swrtRadius;

    return swrtRegion;
}

void GlimmerSWRTTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerTLAS& tlas = *context.tlas;

    // the mask follows whichever TLAS is live, including one swapped in while another technique was active
    if (tlas.IsReady() && (context.tlasSwapped || tlas.GetGeneration() != m_maskGeneration))
    {
        const Vec3f regionCenter = context.region.GetCenter();

        m_footprintMask->Rebuild(context.frame, tlas, Vec2f(regionCenter.x, regionCenter.z), GetSWRTRadius(context.region));

        m_maskGeneration = tlas.GetGeneration();
    }

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        GlimmerSWRTProbeUpdateInputs inputs;
        inputs.viewerPosition = context.channelState->viewerPosition;
        inputs.surfaceCache = context.surfaceCache;
        inputs.spanCache = context.spanCache;
        inputs.blasCache = context.blasCache;
        inputs.tlas = context.tlas;
        inputs.skyProbe = context.skyProbe;

        m_probeVolume->Update(context.frame, inputs);
    }
}

bool GlimmerSWRTTechnique::IsReady() const
{
    return m_probeVolume->IsReady();
}

void GlimmerSWRTTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator) const
{
    // Must match GlimmerTechniqueApply in Shaders/Glimmer/SWRT/GlimmerSWRTApply.hlsli
    GlimmerProbeVolumeShaderData shaderData {};

    if (IsReady())
    {
        shaderData = m_probeVolume->GetShaderData();
    }

    cbufferAllocator.Write(&shaderData);
}

uint32 GlimmerSWRTTechnique::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex) const
{
    if (IsReady())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH0Texture"_sh, m_probeVolume->GetSHImageView(0));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH1Texture"_sh, m_probeVolume->GetSHImageView(1));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH2Texture"_sh, m_probeVolume->GetSHImageView(2));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, m_probeVolume->GetStateImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, m_probeVolume->GetBaseImageView());
    }
    else
    {
        // never sampled: the zeroed constants tell lighting to skip Glimmer
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH0Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH1Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH2Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
    }

    return uniformIndex;
}

bool GlimmerSWRTTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (context.techniqueView <= int(GlimmerSWRTDebugView::None) || context.techniqueView >= int(GlimmerSWRTDebugView::Max))
    {
        return false;
    }

    if (!context.blasCache || !context.tlas || !context.tlas->IsReady() || !context.blasCache->IsReady() || !context.surfaceCache || !context.spanCache)
    {
        return false;
    }

    const GlimmerTLAS& tlas = *context.tlas;
    const GlimmerBLASCache& blasCache = *context.blasCache;

    View* view = context.view;
    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSWRTDebug);

    GlimmerSWRTDebugConstants constants {};
    constants.dimensionsModeInstances = Vec4u(extent.x, extent.y, uint32(context.techniqueView), tlas.GetNumInstances());
    constants.mask = m_footprintMask->GetShaderData();
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.ground = context.surfaceCache->GetGroundShaderData();
    constants.spans = context.spanCache->GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    const GpuImageRef& depthImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage();
    const GpuImageRef& normalsImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage();

    cr << InsertBarrier(depthImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(normalsImage, ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, tlas.GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerInstancesBuffer"_sh, tlas.GetInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceShaderData)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, blasCache.GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
    cr << SetShaderUniform(uniformIndex++, "FootprintMaskBuffer"_sh, m_footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    s_statGlimmerSWRTDebugRays += extent.x * extent.y;

    return true;
}

} // namespace Hyperion
