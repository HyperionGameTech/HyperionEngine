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

#include <Rendering/Glimmer/Surfels/GlimmerSurfelCache.hpp>
#include <Rendering/Glimmer/Surfels/GlimmerSurfelCVars.hpp>

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
    return g_cvGlimmerSurfelsEnabled.Get()
        && g_cvGlimmerTracer.Get() == 0;
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

    doEasing(inOutLight.sun, target.sun);

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
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique && technique->m_shVolume->IsReady())
    {
        shShaderData = technique->m_shVolume->GetShaderData();
    }

    cbufferAllocator.Write(&shShaderData);
}

uint32 GlimmerTechnique::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique)
{
    const GlimmerSHVolume* shVolume = technique ? technique->m_shVolume.Get() : nullptr;

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

void GlimmerTechnique::WriteSurfelApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    GlimmerSurfelShaderData surfelShaderData {};

    if (technique && technique->m_surfelCache->IsReady())
    {
        surfelShaderData = technique->m_surfelCache->GetShaderData();
    }

    const Vec4u surfelParams {
        uint32(MathUtil::Clamp(g_cvGlimmerSurfelsDebug.Get(), 0, int(GlimmerSurfelDebugMode::Max) - 1)),
        uint32(MathUtil::Clamp(g_cvGlimmerReferenceOffset.Get(), 0.0f, 10.0f) * 100.0f + 0.5f),
        0,
        0
    };

    cbufferAllocator.Write(&surfelShaderData);
    cbufferAllocator.Write(&surfelParams);
}

uint32 GlimmerTechnique::BindSurfelApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique)
{
    const GlimmerSurfelCache* surfelCache = technique ? technique->m_surfelCache.Get() : nullptr;

    if (surfelCache && surfelCache->IsReady())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelsBuffer"_sh, surfelCache->GetSurfelsBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelHashBuffer"_sh, surfelCache->GetHashBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelDataBuffer"_sh, surfelCache->GetDataBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerFoliageBuffer"_sh, surfelCache->GetFoliageBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        // HACK: any structured buffer will do. as the zeroed constants keep lighting from reading it anyways
        for (const StringHash name : { "GlimmerSurfelsBuffer"_sh, "GlimmerSurfelHashBuffer"_sh, "GlimmerSurfelDataBuffer"_sh, "GlimmerFoliageBuffer"_sh })
        {
            cr << SetShaderUniform(uniformIndex++, name, RI.namedBuffers[NamedBuffer::Worlds]);
        }
    }

    return uniformIndex;
}

GlimmerTechnique::GlimmerTechnique()
{
    ResetLighting();
}

GlimmerTechnique::~GlimmerTechnique()
{
    EnqueueDeletion(std::move(m_scratchBuffer));
    EnqueueDeletion(std::move(m_skyBinsBuffer));

    for (GpuBufferRef& readbackBuffer : m_skyBinsReadbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }
}

static constexpr uint32 SkyBinsIntervalFrames = 8;

// the sky probe's radiance averaged over each light term, measured every few frames and read back a few frames later
void GlimmerTechnique::UpdateSkyBins(Frame* frame, EnvProbe* skyProbe)
{
    HYP_SCOPE;

    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;

    if (m_skyBinsReadbackTextures[slot] != 0 && m_skyBinsReadbackBuffers[slot].IsValid())
    {
        Vec4f bins[GlimmerLightTerms];

        m_skyBinsReadbackBuffers[slot]->Invalidate();
        m_skyBinsReadbackBuffers[slot]->Read(sizeof(bins), bins);

        GlimmerHelpers::SetGlimmerSkyBins(m_skyBinsReadbackTextures[slot] - 1, bins);

        m_skyBinsReadbackTextures[slot] = 0;
    }

    if (!skyProbe || (m_skyBinsFrame++ % SkyBinsIntervalFrames) != 0)
    {
        return;
    }

    GlimmerSkyShaderData sky;
    EnvProbeShaderData skyProbeData;
    GlimmerHelpers::GetGlimmerSkyShaderData(skyProbe, sky, skyProbeData);

    if (sky.info.x >= 0xFFFFu)
    {
        return;
    }

    if (!m_skyBinsBuffer.IsValid())
    {
        m_skyBinsBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(GlimmerLightTerms * sizeof(Vec4f), sizeof(Vec4f));

        for (GpuBufferRef& readbackBuffer : m_skyBinsReadbackBuffers)
        {
            readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, GlimmerLightTerms * sizeof(Vec4f));
            readbackBuffer->SetIsCpuAccessible(true);
            Check(readbackBuffer->Create());
        }
    }

    CommandRecorder& cr = frame->cr;

    const Vec4u info(sky.info.x, 0u, 0u, 0u);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&info);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_skyBinsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSkyBins")));
    cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(1, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(2, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(3, "OutBins"_sh, m_skyBinsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
    cr << DispatchCompute(Vec3u { 1, 1, 1 });

    GpuBufferRef& readbackBuffer = m_skyBinsReadbackBuffers[slot];

    cr << InsertBarrier(m_skyBinsBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(readbackBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);
    cr << CopyBuffer(m_skyBinsBuffer.Get(), readbackBuffer.Get(), uint32(GlimmerLightTerms * sizeof(Vec4f)));

    m_skyBinsReadbackTextures[slot] = sky.info.x + 1;
}

void GlimmerTechnique::ResetLighting()
{
    m_shOccupancy = MakeUniqueWithAllocator<GlimmerSHOccupancy, GlimmerAllocator>();
    m_shVolume = MakeUniqueWithAllocator<GlimmerSHVolume, GlimmerAllocator>();
    m_relight = MakeUniqueWithAllocator<GlimmerRelight, GlimmerAllocator>();
    m_localLights = MakeUniqueWithAllocator<GlimmerLocalLights, GlimmerAllocator>();
    m_surfelCache = MakeUniqueWithAllocator<GlimmerSurfelCache, GlimmerAllocator>();

    m_relightLightsGeneration = 0;
}

bool GlimmerTechnique::IsReady() const
{
    return IsSurfelsReady() || IsSHReady();
}

bool GlimmerTechnique::IsSHReady() const
{
    return m_shVolume->IsReady();
}

bool GlimmerTechnique::IsSurfelsReady() const
{
    return m_surfelCache->IsReady();
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
        if (!m_scratchBuffer.IsValid())
        {
            m_scratchBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(16, alignof(Vec4f));

#ifdef HYP_RHI_DEBUG_NAMES
            m_scratchBuffer->SetDebugName(NAME("GlimmerScratch"));
#endif
        }

        const bool surfelsEnabled = g_cvGlimmerSurfelsEnabled.Get();

        UpdateSkyBins(context.frame, context.skyProbe);

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

        if (surfelsEnabled)
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
            *m_scratchBuffer);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.lookAhead = context.channelState->lookAhead;
        shInputs.budgetScale = context.channelState->budgetScale;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();
        shInputs.occupancyMaskBuffer = m_shOccupancy->GetMaskBuffer();
        shInputs.isOccupancySettled = m_shOccupancy->IsSettled();
        shInputs.relight = &m_relight->GetShaderData();
        shInputs.sceneInstances = context.sceneInstances;
        shInputs.triangleCache = context.triangleCache;
        shInputs.relightImageView = relightImageView;
        shInputs.surfelCache = m_surfelCache.Get();
        shInputs.skyProbe = context.skyProbe;
        shInputs.cloudPass = context.cloudPass;
        shInputs.light = light;
        shInputs.localLights = m_localLights.Get();

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerSurfelUpdateInputs surfelInputs;
        surfelInputs.viewerPosition = context.channelState->viewerPosition;
        surfelInputs.budgetScale = context.channelState->budgetScale;
        surfelInputs.surfaceCache = context.surfaceCache;
        surfelInputs.spanCache = context.spanCache;
        surfelInputs.triangleCache = context.triangleCache;
        surfelInputs.sceneInstances = context.sceneInstances;
        surfelInputs.shVolume = m_shVolume.Get();
        surfelInputs.relight = m_relight.Get();
        surfelInputs.brickWorld = m_brickWorld.Get();
        surfelInputs.localLights = m_localLights.Get();
        surfelInputs.skyProbe = context.skyProbe;
        surfelInputs.cloudPass = context.cloudPass;
        surfelInputs.light = light;
        surfelInputs.scratchBuffer = m_scratchBuffer.Get();

        m_surfelCache->Update(context.frame, surfelInputs);
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

    if (!m_shOccupancy->GetMaskBuffer().IsValid())
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
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    const GpuImageViewRef& occupancyImageView = m_shOccupancy->GetImageView();

    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancyImageView.IsValid() ? occupancyImageView : RI.placeholderData->GetImageView3D1x1x1R8());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, m_shOccupancy->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });
    
    return true;
}

#pragma region GlimmerTechnique

} // namespace Hyperion
