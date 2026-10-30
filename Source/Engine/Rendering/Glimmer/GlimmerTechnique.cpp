/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeDebug.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerLocalLights.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>

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

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>
#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

namespace {

struct GlimmerHeightfieldDebugConstants
{
    Vec4u dimensionsMode;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
};

HYP_FORCE_INLINE bool UseBrickTracer()
{
    return g_cvGlimmerProbesEnabled.Get()
        && g_cvGlimmerProbesTracer.Get() == 0;
}

void EaseGlimmerLight(
    GlimmerLightShaderData& inOutLight,
    const GlimmerLightShaderData& target,
    float amount)
{
    float maxDifference = 0.0f;
    float maxValue = 0.0f;

    const auto doEasing = [&](Vec4f& value, const Vec4f& targetValue)
    {
        value += (targetValue - value) * amount;

        for (uint32 component = 0; component < 3; component++)
        {
            maxDifference = MathUtil::Max(maxDifference, MathUtil::Abs(targetValue[component] - value[component]));
            maxValue = MathUtil::Max(maxValue, MathUtil::Abs(targetValue[component]));
        }
    };

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        doEasing(inOutLight.bounceWeights[term], target.bounceWeights[term]);
        doEasing(inOutLight.directWeights[term], target.directWeights[term]);
    }

    if (maxDifference <= maxValue * 1e-3f)
    {
        inOutLight = target;
    }
}

} // namespace

#pragma region GlimmerTechnique

GlimmerSceneRegionParams GlimmerTechnique::GetSceneRegionParams()
{
    const float minRadius = GlimmerHelpers::GetGlimmerRegionMinRadius();

    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(minRadius, g_cvGlimmerSpansRadius.Get());
    params.recenterDistance = minRadius * 0.25f;

    return params;
}

void GlimmerTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    GlimmerProbeVolumeShaderData probeShaderData {};
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique)
    {
        if (technique->m_probeVolume->IsReady() && g_cvGlimmerProbesEnabled.Get())
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
    const GlimmerProbeVolume* probeVolume = technique ? technique->m_probeVolume.Get() : nullptr;
    const GlimmerSHVolume* shVolume = technique ? technique->m_shVolume.Get() : nullptr;

    if (probeVolume)
    {
        // placeholders until the volume has buffers
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume->GetSHBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer());
    }
    else
    {
        // HACK: any structured buffer will do. as the zeroed constants keep lighting from reading it anyways
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }

    if (shVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, shVolume->GetDataImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, shVolume->GetStateImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, shVolume->GetRadianceImageView());
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
    }

    return uniformIndex;
}

GlimmerTechnique::GlimmerTechnique()
{
    ResetLighting();
}

GlimmerTechnique::~GlimmerTechnique()
{
    m_scratchBuffer.Shutdown();
}

void GlimmerTechnique::ResetLighting()
{
    m_probeVolume = MakeUniqueWithAllocator<GlimmerProbeVolume, GlimmerAllocator>();
    m_probeDebug = MakeUniqueWithAllocator<GlimmerProbeDebug, GlimmerAllocator>();
    m_shOccupancy = MakeUniqueWithAllocator<GlimmerSHOccupancy, GlimmerAllocator>();
    m_shVolume = MakeUniqueWithAllocator<GlimmerSHVolume, GlimmerAllocator>();
    m_relight = MakeUniqueWithAllocator<GlimmerRelight, GlimmerAllocator>();
    m_localLights = MakeUniqueWithAllocator<GlimmerLocalLights, GlimmerAllocator>();

    m_relightLightsGeneration = 0;
}

bool GlimmerTechnique::IsReady() const
{
    return IsProbesReady() || IsSHReady();
}

bool GlimmerTechnique::IsSHReady() const
{
    return m_shVolume->IsReady();
}

bool GlimmerTechnique::IsProbesReady() const
{
    return m_probeVolume->IsReady() && g_cvGlimmerProbesEnabled.Get();
}

void GlimmerTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerSceneInstances& sceneInstances = *context.sceneInstances;

    const bool shouldUseBricks = g_cvGlimmerBricksEnabled.Get() && (UseBrickTracer() || g_cvGlimmerBricksReflections.Get());

    if (shouldUseBricks && !m_brickWorld)
    {
        m_brickWorld = MakeUniqueWithAllocator<GlimmerBrickWorld, GlimmerAllocator>();
    }
    else if (!shouldUseBricks && m_brickWorld)
    {
        m_brickWorld.Reset();
    }

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        if (!m_scratchBuffer.IsInitialized())
        {
            m_scratchBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32), 4);

#ifdef HYP_RHI_DEBUG_NAMES
            m_scratchBuffer.gpuBuffer->SetDebugName(NAME("GlimmerScratch"));
#endif
        }

        const bool probesEnabled = g_cvGlimmerProbesEnabled.Get();

        GlimmerLightShaderData targetLight;
        GlimmerHelpers::GetGlimmerLightShaderData(context.skyProbe, targetLight);

        const float deltaSeconds = m_lightTime != 0 ? float(PerformanceClock::TimeSince(m_lightTime)) * 0.001f : 0.0f;
        m_lightTime = PerformanceClock::Now();

        const float easeSeconds = g_cvGlimmerSHLightEaseSeconds.Get();

        if (!m_hasLight || easeSeconds <= 0.0f)
        {
            m_light = targetLight;
            m_hasLight = true;
        }
        else
        {
            EaseGlimmerLight(m_light, targetLight, 1.0f - MathUtil::Exp(-deltaSeconds / easeSeconds));
        }

        const GlimmerLightShaderData& light = m_light;

        m_localLights->Update(*context.channelState);

        if (probesEnabled)
        {
            if (m_localLights->GetGeneration() != m_relightLightsGeneration)
            {
                GlimmerSceneChanges changes;

                if (m_localLights->GetChangesSince(m_relightLightsGeneration, changes))
                {
                    for (const GlimmerSceneChange& change : changes)
                    {
                        m_relight->AddChangedArea(Vec2f(change.bounds.min.x, change.bounds.min.z), Vec2f(change.bounds.max.x, change.bounds.max.z));
                    }
                }
                else
                {
                    m_relight->AddChangedArea(Vec2f(-1e6f), Vec2f(1e6f));
                }

                m_relightLightsGeneration = m_localLights->GetGeneration();
            }

            m_relight->Schedule(*context.channelState, *context.surfaceCache, *context.spanCache);
        }
        else
        {
            m_relight->Invalidate();
        }

        const GpuImageViewRef& relightImageView = m_relight->GetImageView();

        context.frame->cr << InsertBarrier(m_relight->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        // far field
        GlimmerLightmapPages lightmapPages;

        if (context.sceneProxies)
        {
            GlimmerHelpers::CollectGlimmerLightmapPages(*context.sceneProxies, lightmapPages);
        }

        m_shOccupancy->Update(
            context.frame,
            context.channelState->viewerPosition,
            context.channelState->lookAhead,
            sceneInstances,
            *context.triangleCache,
            lightmapPages,
            m_scratchBuffer);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.lookAhead = context.channelState->lookAhead;
        shInputs.budgetScale = context.channelState->budgetScale;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();
        shInputs.occupancyMaskBuffer = &m_shOccupancy->GetMaskBuffer();
        shInputs.isOccupancySettled = m_shOccupancy->IsSettled();
        shInputs.relight = &m_relight->GetShaderData();
        shInputs.sceneInstances = context.sceneInstances;
        shInputs.relightImageView = relightImageView;
        shInputs.probeVolume = m_probeVolume.Get();
        shInputs.skyProbe = context.skyProbe;
        shInputs.cloudPass = context.cloudPass;
        shInputs.light = light;
        shInputs.localLights = m_localLights.Get();

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerProbeUpdateInputs probeInputs;
        probeInputs.occupancy = &m_shOccupancy->GetShaderData();
        probeInputs.occupancyImageView = m_shOccupancy->GetImageView();
        probeInputs.viewerPosition = context.channelState->viewerPosition;
        probeInputs.lookAhead = context.channelState->lookAhead;
        probeInputs.budgetScale = context.channelState->budgetScale;
        probeInputs.surfaceCache = context.surfaceCache;
        probeInputs.spanCache = context.spanCache;
        probeInputs.triangleCache = context.triangleCache;
        probeInputs.sceneInstances = context.sceneInstances;
        probeInputs.shVolume = m_shVolume.Get();
        probeInputs.skyProbe = context.skyProbe;
        probeInputs.relight = m_relight.Get();
        probeInputs.brickWorld = m_brickWorld.Get();
        probeInputs.cloudPass = context.cloudPass;
        probeInputs.light = light;
        probeInputs.localLights = m_localLights.Get();
        probeInputs.scratchBuffer = &m_scratchBuffer;

        if (probesEnabled)
        {
            m_probeVolume->Update(context.frame, probeInputs);
        }
    }

    if (context.updateLighting && context.channel && context.surfaceCache && g_cvGlimmerProbesEnabled.Get()
        && (g_cvGlimmerProbesDebug.Get() > int(GlimmerProbeDebugMode::None) || g_cvGlimmerProbesLogStats.Get()))
    {
        m_probeDebug->Update(context.frame, *m_probeVolume, *context.surfaceCache, *m_shOccupancy, *context.channel);
    }
    else
    {
        m_probeDebug->Reset();
    }

    if (m_brickWorld && context.triangleCache)
    {
        const bool hasViewer = context.channelState && context.channelState->hasViewer;

        const Vec3f viewerPosition = hasViewer ? context.channelState->viewerPosition : context.region.GetCenter();
        const Vec3f lookAhead = hasViewer ? context.channelState->lookAhead : Vec3f::Zero();
        const float budgetScale = context.channelState ? context.channelState->budgetScale : 1.0f;

        GlimmerLightShaderData light;
        GlimmerHelpers::GetGlimmerLightShaderData(context.skyProbe, light);

        m_brickWorld->Update(context.frame, viewerPosition, lookAhead, budgetScale, sceneInstances, *context.triangleCache, this, light);
    }
}

bool GlimmerTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (context.techniqueView >= int(GlimmerHeightfieldDebugView::Max))
    {
        const int brickView = context.techniqueView - int(GlimmerHeightfieldDebugView::Max) + 1;

        return m_brickWorld && brickView < int(GlimmerBrickDebugView::Max) && m_brickWorld->RenderDebugView(context, GlimmerBrickDebugView(brickView));
    }

    if (context.techniqueView <= int(GlimmerHeightfieldDebugView::None))
    {
        return false;
    }

    if (!context.triangleCache || !context.sceneInstances || !context.sceneInstances->IsReady() || !context.triangleCache->IsReady() || !context.surfaceCache || !context.spanCache)
    {
        return false;
    }

    if (!m_shOccupancy->GetMaskBuffer().IsInitialized())
    {
        return false;
    }

    View* view = context.view;
    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    GlimmerHeightfieldDebugConstants constants {};
    constants.dimensionsMode = Vec4u(extent.x, extent.y, uint32(context.techniqueView), 0);
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.ground = context.surfaceCache->GetGroundShaderData();
    constants.spans = context.spanCache->GetShaderData();
    constants.occupancy = m_shOccupancy->GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    GpuImage* depthImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage();
    GpuImage* normalsImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage();

    cr << InsertBarrier(depthImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(normalsImage, ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerHeightfieldDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    const GpuImageViewRef& occupancyImageView = m_shOccupancy->GetImageView();

    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancyImageView.IsValid() ? occupancyImageView : RI.placeholderData->GetImageView3D1x1x1R8());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, m_shOccupancy->GetMaskBuffer());

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });
    
    return true;
}

#pragma region GlimmerTechnique

} // namespace Hyperion
