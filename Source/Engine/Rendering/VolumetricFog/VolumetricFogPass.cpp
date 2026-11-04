/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/VolumetricFog/VolumetricFogPass.hpp>

#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>

#include <Rendering/Clouds/CloudPass.hpp>

#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/Light/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Halton.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

CVar<bool> g_cvVolumetricFog { "Rendering.VolumetricFog", true };
CVar<bool> g_cvFogVolumes { "Rendering.FogVolumes", true };
CVar<uint32> g_cvVolumetricFogGridPixelSize { "Rendering.VolumetricFog.GridPixelSize", 8 };
CVar<uint32> g_cvVolumetricFogGridSlices { "Rendering.VolumetricFog.GridSlices", 128 };
CVar<float> g_cvVolumetricFogDistance { "Rendering.VolumetricFog.Distance", 150.0f };
CVar<float> g_cvVolumetricFogHistoryWeight { "Rendering.VolumetricFog.HistoryWeight", 0.9f };
CVar<uint32> g_cvVolumetricFogMultiScatterOctaves { "Rendering.VolumetricFog.MultiScatterOctaves", 3 };

static EngineStatGpuTimer s_statVolumetricFogAmbient("Rendering/GPU/VolumetricFogAmbient");
static EngineStatGpuTimer s_statVolumetricFogInject("Rendering/GPU/VolumetricFogInject");
static EngineStatGpuTimer s_statVolumetricFogTemporal("Rendering/GPU/VolumetricFogTemporal");
static EngineStatGpuTimer s_statVolumetricFogIntegrate("Rendering/GPU/VolumetricFogIntegrate");
static EngineStatGpuTimer s_statVolumetricFogApply("Rendering/GPU/VolumetricFogApply");

static constexpr uint32 NoiseDimensions = 64;
static constexpr float GridNearDepth = 0.5f;
static constexpr uint32 JitterSequenceLength = 16;

struct VolumetricFogNoiseConstants
{
    uint32 dimensions;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

struct VolumetricFogTemporalConstants
{
    Mat4f previousViewProjection;
    VolumetricFogGridShaderData previousGrid;
    Vec4u historyParams;
};

struct VolumetricFogAmbientConstants
{
    Vec4u ambientDimensions;
};

struct VolumetricFogApplyConstants
{
    Vec4u frameParams;
};

static const LightShaderData s_noLightShaderData {};

#pragma region VolumetricFogPass

VolumetricFogPass::VolumetricFogPass(const Vec2u& extent)
    : m_extent(extent),
      m_gridDimensions(Vec3u::Zero()),
      m_ambientDimensions(Vec3u::Zero()),
      m_noiseReady(false),
      m_historyIndex(0),
      m_gridShaderData {},
      m_previousGridShaderData {},
      m_lastRenderedFrame(~0u),
      m_isActive(false)
{
}

VolumetricFogPass::~VolumetricFogPass()
{
    EnqueueDeletion(std::move(m_noiseTexture));
    EnqueueDeletion(std::move(m_ambientTexture));
    EnqueueDeletion(std::move(m_litTexture));
    EnqueueDeletion(std::move(m_historyTextures[0]));
    EnqueueDeletion(std::move(m_historyTextures[1]));
    EnqueueDeletion(std::move(m_integratedTexture));
}

void VolumetricFogPass::Create()
{
    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("VolumetricFogQuad"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
    m_quadMesh->UploadGpuData();
}

void VolumetricFogPass::Resize(const Vec2u& extent)
{
    m_extent = extent;
}

float VolumetricFogPass::GetDistance() const
{
    return MathUtil::Max(g_cvVolumetricFogDistance.Get(), GridNearDepth * 2.0f);
}

void VolumetricFogPass::WriteShaderData(CBufferAllocator& cbufferAllocator) const
{
    if (m_isActive)
    {
        cbufferAllocator.Write(&m_gridShaderData);
    }
    else
    {
        static const VolumetricFogGridShaderData s_inactiveGrid {};
        cbufferAllocator.Write(&s_inactiveGrid);
    }
}

const GpuImageViewRef& VolumetricFogPass::GetIntegratedView() const
{
    if (!m_isActive || !m_integratedTexture.IsValid())
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_integratedTexture.Get());
}

void VolumetricFogPass::CreateNoiseTexture()
{
    m_noiseTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture3D,
        TextureFormat::RGBA8,
        Vec3u { NoiseDimensions, NoiseDimensions, NoiseDimensions },
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat,
        1,
        ImageUsage::Storage | ImageUsage::Sampled });

    m_noiseTexture->SetIsTransient(true);
    m_noiseTexture->SetName(NAME("VolumetricFogNoise"));

    Check(m_noiseTexture->Create());
}

void VolumetricFogPass::GenerateNoise(Frame* frame)
{
    if (!m_noiseTexture.IsValid())
    {
        CreateNoiseTexture();
    }

    VolumetricFogNoiseConstants constants {};
    constants.dimensions = NoiseDimensions;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_noiseTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogNoise")));

    cr << SetShaderUniform(0, "OutNoise"_sh, RI.textureViewCache->GetOrCreate(m_noiseTexture.Get()));
    cr << SetShaderUniform(1, "VolumetricFogNoiseConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { NoiseDimensions / 8, NoiseDimensions / 8, NoiseDimensions / 4 });

    cr << InsertBarrier(m_noiseTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_noiseReady = true;
}

void VolumetricFogPass::CreateGridTextures(const Vec3u& dimensions)
{
    const auto createGridTexture = [](Handle<Texture>& texture, const Vec3u& extent, Name name)
    {
        if (texture.IsValid())
        {
            EnqueueDeletion(std::move(texture));
        }

        texture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture3D,
            TextureFormat::RGBA16F,
            extent,
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled });

        texture->SetIsTransient(true);
        texture->SetName(name);

        Check(texture->Create());
    };

    m_gridDimensions = dimensions;

    // ambient light changes slowly across the volume, so it's lit at a fraction of the grid's resolution
    m_ambientDimensions = Vec3u {
        MathUtil::Max((dimensions.x + 3) / 4, 1u),
        MathUtil::Max((dimensions.y + 3) / 4, 1u),
        MathUtil::Max((dimensions.z + 1) / 2, 1u)
    };

    createGridTexture(m_ambientTexture, m_ambientDimensions, NAME("VolumetricFogAmbient"));
    createGridTexture(m_litTexture, dimensions, NAME("VolumetricFogLit"));
    createGridTexture(m_historyTextures[0], dimensions, NAME("VolumetricFogHistory0"));
    createGridTexture(m_historyTextures[1], dimensions, NAME("VolumetricFogHistory1"));
    createGridTexture(m_integratedTexture, dimensions, NAME("VolumetricFogIntegrated"));

    // the old history is the wrong size to reproject from
    m_lastRenderedFrame = ~0u;
}

void VolumetricFogPass::Update(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view != nullptr);

    m_isActive = false;

    if (!g_cvVolumetricFog.Get())
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    const bool hasGlobalFog = (GetWorldBufferData()->environmentFlags & uint32(WorldEnvironmentFlags::VolumetricFog)) != 0;

    uint32 numFogVolumes = 0;

    if (g_cvFogVolumes.Get())
    {
        RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
        rpl.BeginRead();

        HYP_DEFER({ rpl.EndRead(); });

        numFogVolumes = rpl.GetFogVolumes().NumCurrent();
    }

    if (!hasGlobalFog && numFogVolumes == 0)
    {
        return;
    }

    DeferredPassData* dpd = TryCast<DeferredPassData>(renderSetup.passData);

    // path tracing skips the cluster grid the local lights come from
    if (!dpd || !dpd->gridTilesBuffer || !dpd->gridIndexBuffer || !dpd->cloudPass)
    {
        return;
    }

    if (!m_noiseReady)
    {
        GenerateNoise(frame);
    }

    const uint32 gridPixelSize = MathUtil::Clamp(g_cvVolumetricFogGridPixelSize.Get(), 4u, 32u);
    const uint32 gridSlices = MathUtil::Clamp(g_cvVolumetricFogGridSlices.Get(), 16u, 256u);

    const Vec3u gridDimensions {
        MathUtil::Max((m_extent.x + gridPixelSize - 1) / gridPixelSize, 1u),
        MathUtil::Max((m_extent.y + gridPixelSize - 1) / gridPixelSize, 1u),
        gridSlices
    };

    if (gridDimensions != m_gridDimensions || !m_integratedTexture.IsValid())
    {
        CreateGridTextures(gridDimensions);
    }

    const uint32 frameCounter = GetFrameCounter();
    const uint32 jitterIndex = (frameCounter % JitterSequenceLength) + 1;

    const float farDepth = GetDistance();

    VolumetricFogGridShaderData grid {};
    grid.dimensions = Vec4u(m_gridDimensions, 1u);
    grid.depthParams = Vec4f(GridNearDepth, farDepth, std::log2(farDepth / GridNearDepth), 1.0f / float(m_gridDimensions.z));

    grid.jitter = Vec4f(
        HaltonSequence::GetHalton(jitterIndex, 2) - 0.5f,
        HaltonSequence::GetHalton(jitterIndex, 3) - 0.5f,
        HaltonSequence::GetHalton(jitterIndex, 5) - 0.5f,
        MathUtil::Clamp(g_cvVolumetricFogHistoryWeight.Get(), 0.0f, 0.98f));

    grid.params = Vec4f(float(MathUtil::Clamp(g_cvVolumetricFogMultiScatterOctaves.Get(), 1u, 4u)), 0.0f, 0.0f, 0.0f);

    ComputeAmbient(frame, renderSetup, grid);
    Inject(frame, renderSetup, grid);
    ResolveTemporal(frame, renderSetup, grid);
    Integrate(frame, grid);

    m_previousGridShaderData = grid;
    m_previousViewProjection = cameraProxy->bufferData.viewProjMat;
    m_lastRenderedFrame = frameCounter;

    m_gridShaderData = grid;
    m_isActive = true;
}

void VolumetricFogPass::ComputeAmbient(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& grid)
{
    ENGINE_STAT_GPU_SCOPE(&s_statVolumetricFogAmbient);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    EnvProbeShaderData skyProbeShaderData {};
    bool hasSkyProbe = false;

    const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();

    if (skyProbes.Any())
    {
        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(*skyProbes.Begin())))
        {
            skyProbeShaderData = skyProbeProxy->bufferData;
            hasSkyProbe = true;
        }
    }

    GlimmerPass* glimmerPass = static_cast<GlimmerPass*>(RI.namedPasses[NamedPass::Glimmer][0]);

    VolumetricFogAmbientConstants constants {};
    constants.ambientDimensions = Vec4u(m_ambientDimensions, hasSkyProbe ? 1u : 0u);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&grid);
    RI.cbufferAllocator->Write(&constants);

    const GlimmerScenePassData* glimmerScene = glimmerPass->GetSceneForWorld(renderSetup.world);
    const GlimmerSurfaceCache* surfaceCache = glimmerScene ? glimmerScene->surfaceCache.Get() : nullptr;

    if (surfaceCache)
    {
        RI.cbufferAllocator->Write(&surfaceCache->GetGroundShaderData());
    }
    else
    {
        // zeroed valid rects cover nothing, so the fog keeps absolute heights
        static const GlimmerGroundShaderData s_noGroundShaderData {};
        RI.cbufferAllocator->Write(&s_noGroundShaderData);
    }

    RI.cbufferAllocator->Write(&skyProbeShaderData);
    glimmerPass->WriteApplyShaderData(*RI.cbufferAllocator, renderSetup.world);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_ambientTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogAmbient")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "OutAmbientTexture"_sh, RI.textureViewCache->GetOrCreate(m_ambientTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    if (surfaceCache)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache->GetGroundImageView());
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
    }

    uniformIndex = glimmerPass->BindApplyResources(cr, uniformIndex, renderSetup.world);

    cr << SetShaderUniform(uniformIndex++, "VolumetricFogAmbientConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_ambientDimensions.x + 3) / 4, (m_ambientDimensions.y + 3) / 4, (m_ambientDimensions.z + 3) / 4 });

    cr << InsertBarrier(m_ambientTexture->GetGpuImage(), ResourceState::ShaderResource);
}

void VolumetricFogPass::Inject(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& gridIn)
{
    ENGINE_STAT_GPU_SCOPE(&s_statVolumetricFogInject);

    DeferredPassData* dpd = TryCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    VolumetricFogGridShaderData grid = gridIn;

    const LightShaderData* directionalLightShaderData = &s_noLightShaderData;
    DirectionalLightCSMData directionalCSMData {};

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() != LightType::Directional)
        {
            continue;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));

        if (!lightProxy)
        {
            continue;
        }

        directionalLightShaderData = &lightProxy->bufferData;

        ShadowMap* shadowMaps[MaxShadowMapCascades] {};
        View* shadowMapViewsDynamic[MaxShadowMapCascades] {};
        View* shadowMapViewsStatic[MaxShadowMapCascades] {};

        const uint32 numCascades = MathUtil::Clamp(lightProxy->numCascades, 1u, MaxShadowMapCascades);

        for (uint32 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
        {
            shadowMaps[cascadeIndex] = RI.shadowMapCache->GetShadowMap(
                light,
                renderSetup.view,
                cascadeIndex,
                shadowMapViewsDynamic[cascadeIndex],
                shadowMapViewsStatic[cascadeIndex]);
        }

        DeferredRendererHelpers::FillShadowMapDataCSM(
            &directionalCSMData,
            shadowMapViewsDynamic,
            shadowMapViewsStatic,
            shadowMaps,
            numCascades);

        break;
    }

    VolumetricFogVolumeShaderData fogVolumeData[MaxFogVolumes] {};
    uint32 numFogVolumes = 0;

    for (FogVolume* fogVolume : rpl.GetFogVolumes())
    {
        if (!g_cvFogVolumes.Get() || numFogVolumes >= MaxFogVolumes)
        {
            break;
        }

        RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(fogVolume));

        if (!proxy)
        {
            continue;
        }

        const FogVolumeShaderData& volumeData = proxy->bufferData;

        VolumetricFogVolumeShaderData& outVolume = fogVolumeData[numFogVolumes++];
        outVolume.aabbMin = Vec4f(proxy->worldAabb.min, volumeData.shape.y);
        outVolume.aabbMax = Vec4f(proxy->worldAabb.max, volumeData.medium.x);
        outVolume.lighting = volumeData.lighting;
        outVolume.phase = Vec4f(volumeData.medium.y, volumeData.medium.z, volumeData.medium.w, volumeData.shape.x);
    }

    grid.params.y = float(numFogVolumes);

    if (dpd->clusteredShadowMapIndexBuffer == nullptr)
    {
        dpd->clusteredShadowMapIndexBuffer = &RI.bufferAllocator->AcquireByteAddressBuffer(sizeof(uint32));
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&grid);
    RI.cbufferAllocator->Write(directionalLightShaderData);
    RI.cbufferAllocator->Write(&directionalCSMData);
    dpd->cloudPass->WriteShaderData(*RI.cbufferAllocator);

    for (uint32 shadowMapIndex = 0; shadowMapIndex < MaxClusteredShadowMaps; shadowMapIndex++)
    {
        if (shadowMapIndex < dpd->numClusteredShadowMaps)
        {
            RI.cbufferAllocator->Write(&dpd->clusteredShadowMaps[shadowMapIndex]);
        }
        else
        {
            static const ShadowMapData s_noShadowMapData {};
            RI.cbufferAllocator->Write(&s_noShadowMapData);
        }
    }

    for (uint32 volumeIndex = 0; volumeIndex < MaxFogVolumes; volumeIndex++)
    {
        RI.cbufferAllocator->Write(&fogVolumeData[volumeIndex]);
    }

    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    Sampler* clampedLinearSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge });

    Sampler* repeatSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat });

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_litTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogInject")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, clampedLinearSampler);
    cr << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(uniformIndex++, "SamplerRepeat"_sh, repeatSampler);

    cr << SetShaderUniform(uniformIndex++, "OutLitTexture"_sh, RI.textureViewCache->GetOrCreate(m_litTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "NoiseTexture"_sh, RI.textureViewCache->GetOrCreate(m_noiseTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "AmbientTexture"_sh, RI.textureViewCache->GetOrCreate(m_ambientTexture.Get()));

    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(uniformIndex++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(uniformIndex++, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(uniformIndex++, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);
    cr << SetShaderUniform(uniformIndex++, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
    cr << SetShaderUniform(uniformIndex++, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);
    cr << SetShaderUniform(uniformIndex++, "ShadowMapIndexBuffer"_sh, *dpd->clusteredShadowMapIndexBuffer);

    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, dpd->cloudPass->GetWeatherMapView());
    cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, dpd->cloudPass->GetShadowMapView());

    cr << SetShaderUniform(uniformIndex++, "VolumetricFogConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_gridDimensions.x + 7) / 8, (m_gridDimensions.y + 7) / 8, m_gridDimensions.z });

    cr << InsertBarrier(m_litTexture->GetGpuImage(), ResourceState::ShaderResource);
}

void VolumetricFogPass::ResolveTemporal(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& grid)
{
    ENGINE_STAT_GPU_SCOPE(&s_statVolumetricFogTemporal);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    const bool isHistoryValid = m_lastRenderedFrame != ~0u && GetFrameCounter() == m_lastRenderedFrame + 1;

    VolumetricFogTemporalConstants constants {};
    constants.previousViewProjection = m_previousViewProjection;
    constants.previousGrid = m_previousGridShaderData;
    constants.historyParams = Vec4u(isHistoryValid ? 1u : 0u, 0u, 0u, 0u);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&grid);
    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    Sampler* clampedLinearSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge });

    Texture* historyTexture = m_historyTextures[m_historyIndex].Get();
    Texture* outHistoryTexture = m_historyTextures[m_historyIndex ^ 1u].Get();

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(historyTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogTemporal")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, clampedLinearSampler);
    cr << SetShaderUniform(uniformIndex++, "LitTexture"_sh, RI.textureViewCache->GetOrCreate(m_litTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "HistoryTexture"_sh, RI.textureViewCache->GetOrCreate(historyTexture));
    cr << SetShaderUniform(uniformIndex++, "OutHistoryTexture"_sh, RI.textureViewCache->GetOrCreate(outHistoryTexture));
    cr << SetShaderUniform(uniformIndex++, "VolumetricFogTemporalConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_gridDimensions.x + 7) / 8, (m_gridDimensions.y + 7) / 8, m_gridDimensions.z });

    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_historyIndex ^= 1u;
}

void VolumetricFogPass::Integrate(Frame* frame, const VolumetricFogGridShaderData& grid)
{
    ENGINE_STAT_GPU_SCOPE(&s_statVolumetricFogIntegrate);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&grid);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_integratedTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogIntegrate")));

    cr << SetShaderUniform(0, "MediumTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[m_historyIndex].Get()));
    cr << SetShaderUniform(1, "OutIntegratedTexture"_sh, RI.textureViewCache->GetOrCreate(m_integratedTexture.Get()));
    cr << SetShaderUniform(2, "VolumetricFogIntegrateConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_gridDimensions.x + 7) / 8, (m_gridDimensions.y + 7) / 8, 1 });

    cr << InsertBarrier(m_integratedTexture->GetGpuImage(), ResourceState::ShaderResource);
}

void VolumetricFogPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.view != nullptr && renderSetup.framebuffer != nullptr);

    if (!m_isActive)
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statVolumetricFogApply);

    VolumetricFogApplyConstants constants {};
    constants.frameParams = Vec4u(GetFrameCounter(), 0u, 0u, 0u);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&m_gridShaderData);
    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    Sampler* clampedLinearSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    });

    const FramebufferRef& opaqueFramebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentFramebuffer(renderSetup.framebuffer);
    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);

    // premultiplied: color = inscatter + scene * transmittance, destination alpha untouched
    cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::SrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));

    cr << SetCurrentShader(ShaderDesc(NAME("VolumetricFogApply")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, clampedLinearSampler);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaqueFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "IntegratedTexture"_sh, RI.textureViewCache->GetOrCreate(m_integratedTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "VolumetricFogApplyConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));

    cr << DrawIndexed(6);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentFramebuffer(nullptr);
}

#pragma endregion VolumetricFogPass

} // namespace Hyperion
