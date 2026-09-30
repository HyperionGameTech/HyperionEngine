/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeDebug.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

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

GlimmerSceneRegionParams GlimmerTechnique::GetSceneRegionParams()
{
    const float nearFieldRadius = GetGlimmerNearFieldRadius();

    // SWRT only covers the middle of the region; the wider span region (which the SH volume's occupancy and rays see too) rides along with it
    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(nearFieldRadius, g_cvGlimmerSWRTSpansRadius.Get());
    params.recenterDistance = nearFieldRadius * RegionRecenterFraction;
    params.snap = GlimmerFootprintMaskCellSize;

    return params;
}

void GlimmerTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    // Must match GlimmerApply in Shaders/Glimmer/GlimmerApply.hlsli, after its params and settings
    GlimmerProbeVolumeShaderData probeShaderData {};
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique)
    {
        if (technique->m_probeVolume->IsReady())
        {
            probeShaderData = technique->m_probeVolume->GetShaderData();
        }

        if (technique->m_shVolume->IsReady())
        {
            shShaderData = technique->m_shVolume->GetShaderData();
        }
    }

    cbufferAllocator.Write(&probeShaderData);
    cbufferAllocator.Write(&shShaderData);
}

uint32 GlimmerTechnique::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique)
{
    const GlimmerSWRTProbeVolume* probeVolume = technique ? technique->m_probeVolume.Get() : nullptr;
    const GlimmerSHVolume* shVolume = technique ? technique->m_shVolume.Get() : nullptr;

    // in the order GlimmerSWRTApply.hlsli and GlimmerSHApply.hlsli declare them
    if (probeVolume && probeVolume->IsReady())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHTexture"_sh, probeVolume->GetSHImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, probeVolume->GetStateImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, probeVolume->GetBaseImageView());
    }
    else
    {
        // never sampled: the zeroed constants tell lighting to skip the probes
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
    }

    if (shVolume)
    {
        // the volume hands out placeholders until it has textures; the zeroed constants keep lighting from sampling them
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, shVolume->GetDataImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, shVolume->GetStateImageView());
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
    }

    return uniformIndex;
}

GlimmerTechnique::GlimmerTechnique()
    : m_footprintMask(MakeUnique<GlimmerFootprintMask>()),
      m_probeVolume(MakeUnique<GlimmerSWRTProbeVolume>()),
      m_probeDebug(MakeUnique<GlimmerSWRTProbeDebug>()),
      m_shOccupancy(MakeUnique<GlimmerSHOccupancy>()),
      m_shVolume(MakeUnique<GlimmerSHVolume>()),
      m_maskGeneration(~0u)
{
}

GlimmerTechnique::~GlimmerTechnique()
{
}

BoundingBox GlimmerTechnique::GetTracedRegion(const BoundingBox& sceneRegion) const
{
    const Vec3f regionCenter = sceneRegion.GetCenter();
    const float swrtRadius = GetGlimmerSWRTRadius(sceneRegion);

    BoundingBox swrtRegion = sceneRegion;
    swrtRegion.min.x = regionCenter.x - swrtRadius;
    swrtRegion.max.x = regionCenter.x + swrtRadius;
    swrtRegion.min.z = regionCenter.z - swrtRadius;
    swrtRegion.max.z = regionCenter.z + swrtRadius;

    return swrtRegion;
}

void GlimmerTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerTLAS& tlas = *context.tlas;

    if (tlas.IsReady() && (context.tlasSwapped || tlas.GetGeneration() != m_maskGeneration))
    {
        const Vec3f regionCenter = context.region.GetCenter();

        m_footprintMask->Rebuild(context.frame, tlas, Vec2f(regionCenter.x, regionCenter.z), GetGlimmerSWRTRadius(context.region));

        m_maskGeneration = tlas.GetGeneration();
    }

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        // far field first: the probes' rays that end past the near field take their bounce from it
        m_shOccupancy->Update(context.frame, context.channelState->viewerPosition, tlas, *context.blasCache);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerSWRTProbeUpdateInputs probeInputs;
        probeInputs.viewerPosition = context.channelState->viewerPosition;
        probeInputs.surfaceCache = context.surfaceCache;
        probeInputs.spanCache = context.spanCache;
        probeInputs.blasCache = context.blasCache;
        probeInputs.tlas = context.tlas;
        probeInputs.footprintMask = m_footprintMask.Get();
        probeInputs.shVolume = m_shVolume.Get();
        probeInputs.skyProbe = context.skyProbe;

        m_probeVolume->Update(context.frame, probeInputs);
    }

    if (context.updateLighting && context.channel && g_cvGlimmerSWRTDebugProbes.Get() > int(GlimmerSWRTDebugProbes::None))
    {
        m_probeDebug->Update(context.frame, *m_probeVolume, *context.channel);
    }
    else
    {
        m_probeDebug->Reset();
    }
}

bool GlimmerTechnique::IsReady() const
{
    return m_probeVolume->IsReady() || m_shVolume->IsReady();
}

bool GlimmerTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
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
