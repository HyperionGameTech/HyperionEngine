/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerReference.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerReference("Rendering/GPU/Glimmer/Reference");

static StaticShaderPropertyId s_propModeTrace { ShaderProperty(NAME("MODE"), NAME("TRACE")) };
static StaticShaderPropertyId s_propModeStats { ShaderProperty(NAME("MODE"), NAME("STATS")) };
static StaticShaderPropertyId s_propModeReduce { ShaderProperty(NAME("MODE"), NAME("REDUCE")) };

static constexpr uint32 TraceGroupSize = 8;
static constexpr uint32 StatsGroupSize = 16;

static constexpr uint32 FirstStatsFrames = 16;
static constexpr uint32 StatsFramesStep = 1024;

static constexpr float SunTurnCosine = 0.99999f;

static constexpr const char* ClassNames[GlimmerReferenceClasses] = { "all", "static", "foliage", "dynamic", "terrain" };

// NOTE must match GlimmerReference.hlsl
struct GlimmerReferenceConstants
{
    Vec4u dimensions;
    Vec4u info;
    Vec4u stats;
    Vec4f toSun;
    Vec4f sunIrradiance;
    Vec4f params;
    Vec4f display;
    Vec4f skySH[GlimmerLightTerms];
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
};

static float RoundTo(float value, float step)
{
    return MathUtil::Round(value / step) * step;
}

GlimmerReference::GlimmerReference()
    : m_hasSettings(false),
      m_numFrames(0),
      m_nextStatsFrames(FirstStatsFrames),
      m_seed(0)
{
}

GlimmerReference::~GlimmerReference()
{
    EnqueueDeletion(std::move(m_partialsBuffer));
    EnqueueDeletion(std::move(m_statsBuffer));
    EnqueueDeletion(std::move(m_placeholderBuffer));

    for (GpuBufferRef& readbackBuffer : m_statsReadbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }
}

bool GlimmerReference::IsSameSettings(const Settings& a, const Settings& b)
{
    return memcmp(&a.viewMatrix, &b.viewMatrix, sizeof(Mat4f)) == 0
        && a.extent == b.extent
        && a.toSun.Dot(b.toSun) >= SunTurnCosine
        && (a.sunIrradiance - b.sunIrradiance).Length() <= 1e-3f * MathUtil::Max(a.sunIrradiance.Length(), 1e-3f)
        && a.skyTextureIndex == b.skyTextureIndex
        && a.skyScale == b.skyScale
        && a.isSkyFromSH == b.isSkyFromSH
        && a.isFurnace == b.isFurnace
        && a.paths == b.paths
        && a.stride == b.stride
        && a.offsetCentimetres == b.offsetCentimetres
        && a.bounces == b.bounces
        && a.foliageExtinction == b.foliageExtinction
        && a.sceneGeneration == b.sceneGeneration
        && a.numTracedInstances == b.numTracedInstances;
}

void GlimmerReference::CreateResources(const Vec2u& extent)
{
    m_referenceTexture = GlimmerHelpers::CreateGlimmerStorageTexture(TextureType::Texture2D, TextureFormat::RGBA32F, Vec3u(extent, 1), 1, NAME("GlimmerReference"), TextureWrapMode::ClampToEdge);
    m_glimmerTexture = GlimmerHelpers::CreateGlimmerStorageTexture(TextureType::Texture2D, TextureFormat::RGBA32F, Vec3u(extent, 1), 1, NAME("GlimmerReferenceGlimmer"), TextureWrapMode::ClampToEdge);
    m_shadedReferenceTexture = GlimmerHelpers::CreateGlimmerStorageTexture(TextureType::Texture2D, TextureFormat::RGBA32F, Vec3u(extent, 1), 1, NAME("GlimmerReferenceShaded"), TextureWrapMode::ClampToEdge);
    m_shadedGlimmerTexture = GlimmerHelpers::CreateGlimmerStorageTexture(TextureType::Texture2D, TextureFormat::RGBA32F, Vec3u(extent, 1), 1, NAME("GlimmerReferenceShadedGlimmer"), TextureWrapMode::ClampToEdge);

    if (!m_statsBuffer.IsValid())
    {
        m_statsBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(GlimmerReferenceStatsStride * sizeof(float), sizeof(float));
        m_placeholderBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(Vec4f) * 4, sizeof(Vec4f));

        for (GpuBufferRef& readbackBuffer : m_statsReadbackBuffers)
        {
            readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, GlimmerReferenceStatsStride * sizeof(float));
            readbackBuffer->SetIsCpuAccessible(true);
            Check(readbackBuffer->Create());
        }
    }
}

void GlimmerReference::ReadStats(Frame* frame)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    const uint32 frames = m_statsReadbackFrames[slot];

    if (frames == 0 || !m_statsReadbackBuffers[slot].IsValid())
    {
        return;
    }

    m_statsReadbackFrames[slot] = 0;

    float stats[GlimmerReferenceStatsStride];

    m_statsReadbackBuffers[slot]->Invalidate();
    m_statsReadbackBuffers[slot]->Read(sizeof(stats), stats);

    String line;

    for (uint32 classIndex = 0; classIndex < GlimmerReferenceClasses; classIndex++)
    {
        const float* values = stats + classIndex * GlimmerReferenceClassValues;

        const float pixels = values[0];

        if (pixels < 1.0f)
        {
            continue;
        }

        const float referenceMean = values[1] / pixels;
        const float glimmerMean = values[2] / pixels;

        const float scale = MathUtil::Max(referenceMean, 1e-6f);

        // the error that would remain with both converged: the measured squared error less what their noise adds to it
        const float rmse = MathUtil::Sqrt(MathUtil::Max(values[3] - values[4], 0.0f) / pixels) / scale;
        const float noise = MathUtil::Sqrt(values[4] / pixels) / scale;
        const float colorRmse = MathUtil::Sqrt(values[5] / pixels) / scale;

        const float bias = glimmerMean / scale - 1.0f;

        if (line.Any())
        {
            line += " | ";
        }

        line += HYP_FORMAT("{} {}% px: ref {} glimmer {} bias {}% rmse {}% (noise {}%) rgb {}%",
            ClassNames[classIndex],
            RoundTo(100.0f * pixels / MathUtil::Max(stats[0], 1.0f), 0.1f),
            RoundTo(referenceMean, 0.001f),
            RoundTo(glimmerMean, 0.001f),
            RoundTo(100.0f * bias, 0.1f),
            RoundTo(100.0f * rmse, 0.1f),
            RoundTo(100.0f * noise, 0.1f),
            RoundTo(100.0f * colorRmse, 0.1f));
    }

    HYP_LOG(Rendering, Info, "Glimmer reference at {} frames: {}", frames, line);
}

void GlimmerReference::DispatchStats(const GlimmerDebugViewContext& context, const void* constants, size_t constantsSize, uint32 numGroups)
{
    CommandRecorder& cr = context.frame->cr;

    const size_t partialsSize = size_t(numGroups) * GlimmerReferenceStatsStride * sizeof(float);

    if (!m_partialsBuffer.IsValid() || m_partialsBuffer->Size() < partialsSize)
    {
        EnqueueDeletion(std::move(m_partialsBuffer));

        m_partialsBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(partialsSize, sizeof(float));
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(constants, constantsSize, alignof(Vec4f));
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;

    cr << InsertBarrier(m_referenceTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_glimmerTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_partialsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(s_propModeStats);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerReference"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(context.view->GetCamera()));
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerReferenceTexture"_sh, RI.textureViewCache->GetOrCreate(m_referenceTexture));
        cr << SetShaderUniform(uniformIndex++, "GlimmerAccumulatedTexture"_sh, RI.textureViewCache->GetOrCreate(m_glimmerTexture));
        cr << SetShaderUniform(uniformIndex++, "OutPartials"_sh, m_partialsBuffer.Get(), ShaderDataOffset(0, sizeof(float)));

        const Vec2u extent = context.extent;

        cr << DispatchCompute(Vec3u { (extent.x + StatsGroupSize - 1) / StatsGroupSize, (extent.y + StatsGroupSize - 1) / StatsGroupSize, 1 });
    }

    cr << InsertBarrier(m_partialsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_statsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(s_propModeReduce);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerReference"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GlimmerReferencePartialsBuffer"_sh, m_partialsBuffer.Get(), ShaderDataOffset(0, sizeof(float)));
        cr << SetShaderUniform(uniformIndex++, "OutStats"_sh, m_statsBuffer.Get(), ShaderDataOffset(0, sizeof(float)));

        cr << DispatchCompute(Vec3u { 1, 1, 1 });
    }

    const uint32 slot = context.frame->GetFrameIndex() % NumFramesInFlight;
    GpuBufferRef& readbackBuffer = m_statsReadbackBuffers[slot];

    cr << InsertBarrier(m_statsBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(readbackBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);

    cr << CopyBuffer(m_statsBuffer.Get(), readbackBuffer.Get(), uint32(GlimmerReferenceStatsStride * sizeof(float)));

    cr << InsertBarrier(m_statsBuffer.Get(), ResourceState::ShaderResource);

    m_statsReadbackFrames[slot] = m_numFrames;
}

bool GlimmerReference::Render(const GlimmerDebugViewContext& context)
{
    HYP_SCOPE;

    if (!context.triangleCache || !context.sceneInstances || !context.surfaceCache || !context.spanCache
        || !context.irradianceImageView.IsValid() || !context.sceneInstances->IsReady() || !context.triangleCache->IsReady())
    {
        return false;
    }

    const WorldShaderData* worldData = GetWorldBufferData();
    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(context.view->GetCamera()));

    if (!worldData || !cameraProxy)
    {
        return false;
    }

    ReadStats(context.frame);

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerReference);

    GlimmerSkyShaderData skyData;
    EnvProbeShaderData skyProbeData;
    GlimmerHelpers::GetGlimmerSkyShaderData(context.skyProbe, skyData, skyProbeData);

    const Vec3f toSun = GlimmerHelpers::GetGlimmerSunDirection();
    const float aboveHorizon = MathUtil::Clamp((toSun.y + 0.1f) / 0.15f, 0.0f, 1.0f);

    Settings settings;
    settings.viewMatrix = cameraProxy->bufferData.viewMat;
    settings.extent = context.extent;
    settings.toSun = toSun;
    settings.sunIrradiance = worldData->sunColor.GetXYZ() * worldData->sunAtmosphereTint.GetXYZ() * (worldData->sunDirectionIntensity.w * aboveHorizon);
    settings.skyTextureIndex = skyData.info.x;
    settings.skyScale = skyData.params.x * worldData->skyLightParams.x;
    settings.isSkyFromSH = g_cvGlimmerReferenceSkySH.Get();
    settings.isFurnace = g_cvGlimmerReferenceFurnace.Get();
    settings.paths = uint32(MathUtil::Clamp(g_cvGlimmerReferencePaths.Get(), 1, 64));
    settings.stride = uint32(MathUtil::Clamp(g_cvGlimmerReferenceStride.Get(), 1, 16));
    settings.offsetCentimetres = uint32(MathUtil::Clamp(g_cvGlimmerReferenceOffset.Get(), 0.0f, 10.0f) * 100.0f + 0.5f);
    settings.bounces = uint32(MathUtil::Clamp(g_cvGlimmerReferenceBounces.Get(), 1, 64));
    settings.foliageExtinction = GlimmerHelpers::GetGlimmerFoliageExtinction();
    settings.sceneGeneration = context.sceneInstances->GetGeneration();
    settings.numTracedInstances = context.sceneInstances->GetNumTracedInstances();

    if (!m_hasSettings || !IsSameSettings(settings, m_settings))
    {
        m_numFrames = 0;
        m_nextStatsFrames = FirstStatsFrames;

        for (uint32& frames : m_statsReadbackFrames)
        {
            frames = 0;
        }
    }

    m_settings = settings;
    m_hasSettings = true;

    if (!m_referenceTexture.IsValid() || m_referenceTexture->GetExtent().GetXY() != context.extent)
    {
        CreateResources(context.extent);

        m_numFrames = 0;
        m_nextStatsFrames = FirstStatsFrames;
    }

    const uint32 numTracedInstances = settings.numTracedInstances;

    GlimmerReferenceConstants constants {};
    constants.dimensions = Vec4u(context.extent.x, context.extent.y, ++m_seed * 0x9E3779B9u, m_numFrames);
    constants.info = Vec4u(settings.skyTextureIndex, numTracedInstances, uint32(MathUtil::Clamp(g_cvGlimmerReference.Get(), 1, 4)), settings.paths);
    constants.stats = Vec4u(0, 0, settings.stride, settings.offsetCentimetres);
    constants.toSun = Vec4f(toSun, 0.0f);
    constants.sunIrradiance = Vec4f(settings.sunIrradiance, 0.0f);
    constants.params = Vec4f(settings.foliageExtinction, settings.skyScale, float(settings.bounces), GlimmerDefaultGroundAlbedo);
    constants.display = Vec4f(MathUtil::Max(g_cvGlimmerReferenceExposure.Get(), 0.0f), MathUtil::Max(g_cvGlimmerReferenceErrorScale.Get(), 1e-3f), settings.isFurnace ? 1.0f : 0.0f, g_cvGlimmerReferenceShaded.Get() ? 1.0f : 0.0f);

    if (settings.isSkyFromSH)
    {
        GlimmerLightShaderData light;
        GlimmerHelpers::GetGlimmerLightShaderData(context.skyProbe, light);

        for (uint32 term = 0; term < GlimmerLightTerms; term++)
        {
            constants.skySH[term] = Vec4f(light.directWeights[term].GetXYZ(), term == 0 ? 1.0f : 0.0f);
        }
    }
    constants.ground = context.surfaceCache->GetGroundShaderData();
    constants.spans = context.spanCache->GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;

    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::MatData)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Color)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(context.irradianceImageView->GetImage().Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_referenceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_glimmerTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_shadedReferenceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_shadedGlimmerTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propModeTrace);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerReference"), shaderProperties));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(context.view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));

    if (numTracedInstances != 0)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, context.sceneInstances->GetTLASNodesBuffer().Get(), ShaderDataOffset(0, sizeof(SAHBVHNode)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerTracedInstancesBuffer"_sh, context.sceneInstances->GetTracedInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTracedInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, context.triangleCache->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBLASNode)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, context.triangleCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        for (const StringHash name : { "GlimmerTLASNodesBuffer"_sh, "GlimmerTracedInstancesBuffer"_sh, "GlimmerBLASNodesBuffer"_sh, "GlimmerTrianglesBuffer"_sh })
        {
            cr << SetShaderUniform(uniformIndex++, name, m_placeholderBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        }
    }

    cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(uniformIndex++, "GlimmerIrradianceTexture"_sh, context.irradianceImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferAlbedoTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "OutReference"_sh, RI.textureViewCache->GetOrCreate(m_referenceTexture));
    cr << SetShaderUniform(uniformIndex++, "OutGlimmer"_sh, RI.textureViewCache->GetOrCreate(m_glimmerTexture));
    cr << SetShaderUniform(uniformIndex++, "OutShadedReference"_sh, RI.textureViewCache->GetOrCreate(m_shadedReferenceTexture));
    cr << SetShaderUniform(uniformIndex++, "OutShadedGlimmer"_sh, RI.textureViewCache->GetOrCreate(m_shadedGlimmerTexture));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);

    cr << DispatchCompute(Vec3u { (context.extent.x + TraceGroupSize - 1) / TraceGroupSize, (context.extent.y + TraceGroupSize - 1) / TraceGroupSize, 1 });

    m_numFrames++;

    if (m_numFrames >= m_nextStatsFrames)
    {
        m_nextStatsFrames = m_nextStatsFrames < StatsFramesStep ? m_nextStatsFrames * 4 : m_nextStatsFrames + StatsFramesStep;

        const uint32 groupsX = (context.extent.x + StatsGroupSize - 1) / StatsGroupSize;
        const uint32 groupsY = (context.extent.y + StatsGroupSize - 1) / StatsGroupSize;

        GlimmerReferenceConstants statsConstants = constants;
        statsConstants.dimensions.w = m_numFrames;
        statsConstants.stats = Vec4u(groupsX * groupsY, groupsX, settings.stride, 0);

        DispatchStats(context, &statsConstants, sizeof(statsConstants), groupsX * groupsY);
    }

    cr << InsertBarrier(m_referenceTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_glimmerTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    return true;
}

} // namespace Hyperion
