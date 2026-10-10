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
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

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

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerSWRTDebug("Rendering/GPU/Glimmer/SWRTDebug");

static EngineStatCounter<uint32> s_statGlimmerSWRTDebugRays("Rendering/Glimmer/SWRTDebugRays");

static EngineStatCounter<uint32> s_statGlimmerLightingChanges("Rendering/Glimmer/LightingChanges", false);

static constexpr float RegionRecenterFraction = 0.25f;

// how far the sun brightness  must move before the caches are woken for it
static constexpr float LightingChangeFraction = 0.03f;
// clouds move the sky's brightness all the time, and the probes follow a slow change by themselves
static constexpr float SkyChangeFraction = 0.1f;

struct GlimmerSWRTDebugConstants
{
    Vec4u dimensionsModeInstances;
    GlimmerFootprintMaskShaderData mask;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
};

#pragma region GlimmerTechnique

GlimmerSceneRegionParams GlimmerTechnique::GetSceneRegionParams()
{
    const float nearFieldRadius = GetGlimmerNearFieldRadius();

    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(nearFieldRadius, g_cvGlimmerSWRTSpansRadius.Get());
    params.recenterDistance = nearFieldRadius * RegionRecenterFraction;
    params.snap = GlimmerFootprintMaskCellSize;

    return params;
}

void GlimmerTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    GlimmerProbeVolumeShaderData probeShaderData {};
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique)
    {
        if (technique->m_probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get())
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

    if (probeVolume)
    {
        // placeholders until the volume has buffers
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume->GetSHBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
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
    : m_footprintMask(MakeUnique<GlimmerFootprintMask>()),
      m_probeVolume(MakeUnique<GlimmerSWRTProbeVolume>()),
      m_probeDebug(MakeUnique<GlimmerSWRTProbeDebug>()),
      m_shOccupancy(MakeUnique<GlimmerSHOccupancy>()),
      m_shVolume(MakeUnique<GlimmerSHVolume>()),
      m_relight(MakeUnique<GlimmerRelight>()),
      m_maskGeneration(~0u),
      m_litSkyLight(0.0f),
      m_litSkyLuminance(0.0f),
      m_hasLitSignature(false)
{
}

EnumFlags<GlimmerLightingChangeFlags> GlimmerTechnique::UpdateLightingSignature(EnvProbe* skyProbe)
{
    const WorldShaderData* worldData = GetWorldBufferData();

    if (!worldData)
    {
        return GlimmerLightingChangeFlags::None;
    }

    GlimmerSkyShaderData skyData;
    EnvProbeShaderData skyProbeData;
    GetGlimmerSkyShaderData(skyProbe, skyData, skyProbeData);

    const Vec3f toSun = worldData->sunDirectionIntensity.GetXYZ();
    const Vec3f sunDirection = toSun.LengthSquared() > 1e-8f ? toSun.Normalized() : Vec3f(0.0f, 1.0f, 0.0f);
    const Vec3f sunRadiance = worldData->sunColor.GetXYZ() * worldData->sunAtmosphereTint.GetXYZ() * worldData->sunDirectionIntensity.w;
    const float skyLight = worldData->skyLightParams.x;
    const float skyLuminance = skyData.params.z;

    const auto hasMoved = [](float previous, float current, float fraction = LightingChangeFraction)
    {
        return MathUtil::Abs(current - previous) > fraction * MathUtil::Max(MathUtil::Abs(previous), MathUtil::Abs(current)) + 1e-6f;
    };

    const float cosThreshold = MathUtil::Cos(MathUtil::DegToRad(MathUtil::Clamp(g_cvGlimmerSunAngleThreshold.Get(), 0.01f, 45.0f)));

    EnumFlags<GlimmerLightingChangeFlags> changes = GlimmerLightingChangeFlags::None;

    if (!m_hasLitSignature
        || sunDirection.Dot(m_litSunDirection) < cosThreshold
        || hasMoved(m_litSunRadiance.x, sunRadiance.x)
        || hasMoved(m_litSunRadiance.y, sunRadiance.y)
        || hasMoved(m_litSunRadiance.z, sunRadiance.z))
    {
        changes |= GlimmerLightingChangeFlags::Sun;
    }

    if (!m_hasLitSignature
        || hasMoved(m_litSkyLight, skyLight)
        || hasMoved(m_litSkyLuminance, skyLuminance, SkyChangeFraction))
    {
        changes |= GlimmerLightingChangeFlags::Sky;
    }

    if (!changes)
    {
        return GlimmerLightingChangeFlags::None;
    }

    if (g_cvGlimmerSWRTProbesLogStats.Get())
    {
        HYP_LOG(Rendering, Info, "Glimmer lighting changed: sun turned {} deg, sun radiance {} -> {}, sky light {} -> {}, sky luminance {} -> {}",
            MathUtil::RadToDeg(MathUtil::Arccos(MathUtil::Clamp(sunDirection.Dot(m_litSunDirection), -1.0f, 1.0f))),
            m_litSunRadiance.Sum(), sunRadiance.Sum(), m_litSkyLight, skyLight, m_litSkyLuminance, skyLuminance);
    }

    m_litSunDirection = sunDirection;
    m_litSunRadiance = sunRadiance;
    m_litSkyLight = skyLight;
    m_litSkyLuminance = skyLuminance;
    m_hasLitSignature = true;

    s_statGlimmerLightingChanges++;

    return true;
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

bool GlimmerTechnique::IsReady() const
{
    return (m_probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get()) || m_shVolume->IsReady();
}

void GlimmerTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerTLAS& tlas = *context.tlas;

    if (tlas.IsReady() && (context.tlasSwapped || tlas.GetGeneration() != m_maskGeneration))
    {
        const BoundingBox& gatherRegion = tlas.GetActiveRegion();
        const Vec3f regionCenter = gatherRegion.GetCenter();

        m_footprintMask->Rebuild(context.frame, tlas, Vec2f(regionCenter.x, regionCenter.z), GetGlimmerSWRTRadius(gatherRegion));

        m_maskGeneration = tlas.GetGeneration();
    }

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        const bool areProbesEnabled = g_cvGlimmerSWRTProbesEnabled.Get();

        const EnumFlags<GlimmerLightingChangeFlags> lightingChanges = UpdateLightingSignature(context.skyProbe);

        if (areProbesEnabled)
        {
            m_relight->Schedule(*context.channelState, *context.surfaceCache, *context.spanCache, lightingChanges);
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
            CollectGlimmerLightmapPages(*context.sceneProxies, lightmapPages);
        }

        m_shOccupancy->Update(context.frame, context.channelState->viewerPosition, tlas, *context.blasCache, lightmapPages);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();
        shInputs.occupancyMaskBuffer = m_shOccupancy->GetMaskBuffer();
        shInputs.isOccupancySettled = m_shOccupancy->IsSettled();
        shInputs.relight = &m_relight->GetShaderData();
        shInputs.tlas = context.tlas;
        shInputs.relightImageView = relightImageView;
        shInputs.probeVolume = m_probeVolume.Get();
        shInputs.skyProbe = context.skyProbe;
        shInputs.cloudPass = context.cloudPass;
        shInputs.lightingChanges = lightingChanges;

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerSWRTProbeUpdateInputs probeInputs;
        probeInputs.occupancy = &m_shOccupancy->GetShaderData();
        probeInputs.occupancyImageView = m_shOccupancy->GetImageView();
        probeInputs.viewerPosition = context.channelState->viewerPosition;
        probeInputs.surfaceCache = context.surfaceCache;
        probeInputs.spanCache = context.spanCache;
        probeInputs.blasCache = context.blasCache;
        probeInputs.tlas = context.tlas;
        probeInputs.footprintMask = m_footprintMask.Get();
        probeInputs.shVolume = m_shVolume.Get();
        probeInputs.skyProbe = context.skyProbe;
        probeInputs.relight = m_relight.Get();
        probeInputs.cloudPass = context.cloudPass;
        probeInputs.lightingChanges = lightingChanges;

        if (areProbesEnabled)
        {
            m_probeVolume->Update(context.frame, probeInputs);
        }
    }

    if (context.updateLighting && context.channel && context.surfaceCache && g_cvGlimmerSWRTProbesEnabled.Get()
        && (g_cvGlimmerSWRTDebugProbes.Get() > int(GlimmerSWRTDebugProbes::None) || g_cvGlimmerSWRTProbesLogStats.Get()))
    {
        m_probeDebug->Update(context.frame, *m_probeVolume, *context.surfaceCache, *m_shOccupancy, *context.channel);
    }
    else
    {
        m_probeDebug->Reset();
    }
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

    if (!m_shOccupancy->GetMaskBuffer().IsValid())
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
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, blasCache.GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBLASNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "FootprintMaskBuffer"_sh, m_footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    // the occupancy's zeroed constants (before it's built) have every cascade read as missing
    const GpuImageViewRef& occupancyImageView = m_shOccupancy->GetImageView();
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancyImageView.IsValid() ? occupancyImageView : RI.placeholderData->GetImageView3D1x1x1R8());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, m_shOccupancy->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    s_statGlimmerSWRTDebugRays += extent.x * extent.y;

    return true;
}

#pragma region GlimmerTechnique

} // namespace Hyperion
