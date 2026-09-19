/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Clouds/CloudPass.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Sky/CloudEffectVolume.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

///////////////////////////////////////////////////////////
////////// https://www.jpgrenier.org/clouds.html //////////
///////////////////////////////////////////////////////////

namespace Hyperion {

extern uint32 GetFrameCounter();

CVar<bool> g_cvClouds { "Rendering.Clouds", true };
CVar<bool> g_cvCloudsDebugShadows { "Rendering.Clouds.DebugShadows", false };

CVar<uint32> g_cvCloudsTraceSteps { "Rendering.Clouds.TraceSteps", 48 };
CVar<uint32> g_cvCloudsLightSteps { "Rendering.Clouds.LightSteps", 6 };

// seconds between sky probe recaptures while clouds are active, so ambient and reflections follow them
CVar<float> g_cvCloudsSkyProbeRefreshSeconds { "Rendering.Clouds.SkyProbeRefreshSeconds", 2.0f };

static EngineStatGpuTimer s_statCloudWeatherMap("Rendering/GPU/CloudWeatherMap");
static EngineStatGpuTimer s_statCloudNoise("Rendering/GPU/CloudNoiseGenerate");
static EngineStatGpuTimer s_statCloudShadowMap("Rendering/GPU/CloudShadowMap");
static EngineStatGpuTimer s_statCloudSkyProbe("Rendering/GPU/CloudSkyProbe");
static EngineStatGpuTimer s_statCloudTrace("Rendering/GPU/CloudTrace");
static EngineStatGpuTimer s_statCloudReconstruct("Rendering/GPU/CloudReconstruct");
static EngineStatGpuTimer s_statCloudComposite("Rendering/GPU/CloudComposite");

static StaticShaderPropertyId s_propNoiseModeShape { ShaderProperty(NAME("MODE"), NAME("SHAPE")) };
static StaticShaderPropertyId s_propNoiseModeDetail { ShaderProperty(NAME("MODE"), NAME("DETAIL")) };

// ordered 2x2 pattern, so any four consecutive frames trace every pixel of a block once
static const Vec2u TraceOffsetSequence[4] = {
    Vec2u { 0, 0 },
    Vec2u { 1, 1 },
    Vec2u { 1, 0 },
    Vec2u { 0, 1 }
};

// matches CloudNoiseConstants in Shaders/Clouds/CloudNoiseGenerate.hlsl
struct CloudNoiseConstants
{
    uint32 dimensions;
    uint32 sliceStart;
    uint32 sliceCount;
    uint32 _pad0;
};

// matches CloudNoiseDownsampleConstants in Shaders/Clouds/CloudNoiseDownsample.hlsl
struct CloudNoiseDownsampleConstants
{
    Vec4u sourceDimensions;
    Vec4u targetDimensions;
};

// matches the tail of CloudSkyProbeConstants in Shaders/Clouds/CloudSkyProbe.hlsl
struct CloudSkyProbeConstants
{
    Vec4f rayOrigin;

    uint32 dimensions;
    uint32 traceSteps;
    uint32 lightSteps;
    uint32 hasSun;
};

// matches the tail of CloudTraceConstants in Shaders/Clouds/CloudTrace.hlsl
struct CloudTraceConstants
{
    Vec2u traceDimensions;
    uint32 traceSteps;
    uint32 lightSteps;

    uint32 frameCounter;
    uint32 hasSun;
    Vec2u traceOffset;

    Vec2u historyDimensions;
    uint32 _pad0;
    uint32 _pad1;
};

// matches the tail of CloudReconstructConstants in Shaders/Clouds/CloudReconstruct.hlsl
struct CloudReconstructConstants
{
    Mat4f previousViewProjection;

    Vec2u traceDimensions;
    Vec2u historyDimensions;

    Vec2u traceOffset;
    uint32 historyValid;
    uint32 _pad0;
};

CloudPass::CloudPass(const Vec2u& extent)
    : m_extent(extent),
      m_historyIndex(0),
      m_lastRenderedFrame(~0u),
      m_shapeNoiseSlicesGenerated(0),
      m_noiseReady(false),
      m_weatherMapShaderData {},
      m_generatedCloudData {},
      m_previousKeyframeEvolutionTime(0.0f),
      m_hasGeneratedWeatherMap(false),
      m_shadowMapShaderData {},
      m_shadowMapNextRow(0),
      m_cloudVolumeShaderData {},
      m_cameraPosition(Vec3f::Zero()),
      m_isActive(false)
{
    m_weatherMapShaderData.previousKeyframeSlice = 0;
    m_weatherMapShaderData.nextKeyframeSlice = 1;
}

CloudPass::~CloudPass()
{
    EnqueueDeletion(std::move(m_traceTexture));
    EnqueueDeletion(std::move(m_traceDistanceTexture));

    for (uint32 historySlot = 0; historySlot < 2; historySlot++)
    {
        EnqueueDeletion(std::move(m_historyTextures[historySlot]));
        EnqueueDeletion(std::move(m_historyDistanceTextures[historySlot]));
    }

    EnqueueDeletion(std::move(m_shapeNoise));
    EnqueueDeletion(std::move(m_detailNoise));
    EnqueueDeletion(std::move(m_weatherMap));
    EnqueueDeletion(std::move(m_shadowMap));
}

void CloudPass::Create()
{
    CreateTraceTextures();

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("CloudCompositeQuad"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
    m_quadMesh->UploadGpuData();
}

void CloudPass::Resize(const Vec2u& extent)
{
    if (extent == m_extent)
    {
        return;
    }

    m_extent = extent;

    CreateTraceTextures();
}

void CloudPass::Update(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    m_isActive = false;
    m_weatherMapShaderData.debugShadows = g_cvCloudsDebugShadows.Get() ? 1u : 0u;

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    const RenderProxyEffectVolume* cloudVolumeProxy = nullptr;

    for (EffectVolume* effectVolume : rpl.GetEffectVolumes().GetElements<CloudEffectVolume>())
    {
        cloudVolumeProxy = static_cast<RenderProxyEffectVolume*>(GetRenderProxy(effectVolume));

        if (cloudVolumeProxy)
        {
            break;
        }
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));

    if (!g_cvClouds.Get() || !cloudVolumeProxy || !cameraProxy || !cloudVolumeProxy->bufferData.GetParams<CloudVolumeShaderData>().enabled)
    {
        // the shadow map stops following wind and evolution while inactive, so it's rebuilt from scratch when clouds come back
        m_shadowMapShaderData.isValid = 0;

        return;
    }

    m_cloudVolumeShaderData = cloudVolumeProxy->bufferData;
    m_cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();

    UpdateNoise(frame);
    UpdateWeatherMap(frame, *cloudVolumeProxy);

    m_isActive = m_hasGeneratedWeatherMap;

    Vec3f directionToSun = Vec3f::Zero();

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() != LightType::Directional)
        {
            continue;
        }

        if (RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light)))
        {
            directionToSun = lightProxy->bufferData.positionIntensity.GetXYZ();

            break;
        }
    }

    UpdateShadowMap(frame, *cloudVolumeProxy, directionToSun);
}

void CloudPass::WriteShaderData(CBufferAllocator& cbufferAllocator) const
{
    if (m_isActive)
    {
        cbufferAllocator.Write(&m_cloudVolumeShaderData);
    }
    else
    {
        // zeroed params read as disabled, so shaders skip clouds
        static const EffectVolumeShaderData s_disabledCloudVolume {};
        cbufferAllocator.Write(&s_disabledCloudVolume);
    }

    cbufferAllocator.Write(&m_weatherMapShaderData);

    CloudShadowMapShaderData shadowMapShaderData = m_shadowMapShaderData;
    shadowMapShaderData.isValid = (m_isActive && m_shadowMapShaderData.isValid) ? 1u : 0u;

    cbufferAllocator.Write(&shadowMapShaderData);
}

#pragma region Noise

void CloudPass::CreateNoiseTextures()
{
    const auto createNoiseTexture = [](uint32 dimensions, Name name)
    {
        Handle<Texture> noiseTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture3D,
            TextureFormat::RGBA8,
            Vec3u { dimensions, dimensions, dimensions },
            TextureFilterMode::LinearMipmap,
            TextureFilterMode::Linear,
            TextureWrapMode::Repeat,
            1,
            ImageUsage::Storage | ImageUsage::Sampled
        });

        noiseTexture->SetIsTransient(true);
        noiseTexture->SetName(name);

        Check(noiseTexture->Create());

        return noiseTexture;
    };

    m_shapeNoise = createNoiseTexture(ShapeNoiseDimensions, NAME("CloudShapeNoise"));
    m_detailNoise = createNoiseTexture(DetailNoiseDimensions, NAME("CloudDetailNoise"));
}

void CloudPass::UpdateNoise(Frame* frame)
{
    if (m_noiseReady)
    {
        return;
    }

    if (!m_shapeNoise.IsValid())
    {
        CreateNoiseTextures();

        // a few thousand texels - cheap enough to fill in one go
        GenerateNoiseSlices(frame, m_detailNoise.Get(), /* isShapeNoise */ false, 0, DetailNoiseDimensions);
    }

    if (m_shapeNoiseSlicesGenerated < ShapeNoiseDimensions)
    {
        const uint32 sliceCount = MathUtil::Min(ShapeNoiseSlicesPerFrame, ShapeNoiseDimensions - m_shapeNoiseSlicesGenerated);

        GenerateNoiseSlices(frame, m_shapeNoise.Get(), /* isShapeNoise */ true, m_shapeNoiseSlicesGenerated, sliceCount);

        m_shapeNoiseSlicesGenerated += sliceCount;

        return;
    }

    GenerateNoiseMips(frame, m_shapeNoise.Get());
    GenerateNoiseMips(frame, m_detailNoise.Get());

    m_noiseReady = true;
}

void CloudPass::GenerateNoiseSlices(Frame* frame, Texture* noiseTexture, bool isShapeNoise, uint32 sliceStart, uint32 sliceCount)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudNoise);

    const uint32 dimensions = noiseTexture->GetTextureDesc().extent.x;

    CloudNoiseConstants constants {};
    constants.dimensions = dimensions;
    constants.sliceStart = sliceStart;
    constants.sliceCount = sliceCount;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(isShapeNoise ? s_propNoiseModeShape : s_propNoiseModeDetail);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(noiseTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudNoiseGenerate"), shaderProperties));

    cr << SetShaderUniform(0, "OutNoise"_sh, RI.textureViewCache->GetOrCreate(noiseTexture, 0, 1));
    cr << SetShaderUniform(1, "CloudNoiseConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (dimensions + 7) / 8, (dimensions + 7) / 8, (sliceCount + 3) / 4 });
}

void CloudPass::GenerateNoiseMips(Frame* frame, Texture* noiseTexture)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudNoise);

    const TextureDesc& textureDesc = noiseTexture->GetTextureDesc();
    const uint8 numMips = textureDesc.NumMips();

    CommandRecorder& cr = frame->cr;

    for (uint8 mip = 1; mip < numMips; mip++)
    {
        const uint8 sourceMip = mip - 1;

        const Vec3u sourceExtent = textureDesc.GetMipExtent(sourceMip);
        const Vec3u targetExtent = textureDesc.GetMipExtent(mip);

        CloudNoiseDownsampleConstants constants {};
        constants.sourceDimensions = Vec4u { sourceExtent.x, sourceExtent.y, sourceExtent.z, 0 };
        constants.targetDimensions = Vec4u { targetExtent.x, targetExtent.y, targetExtent.z, 0 };

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        cr << InsertBarrier(noiseTexture->GetGpuImage(), ResourceState::ShaderResource, ImageSubResource { .baseMipLevel = sourceMip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });
        cr << InsertBarrier(noiseTexture->GetGpuImage(), ResourceState::UnorderedAccess, ImageSubResource { .baseMipLevel = mip, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });

        cr << SetCurrentShader(ShaderDesc(NAME("CloudNoiseDownsample")));

        cr << SetShaderUniform(0, "InNoise"_sh, RI.textureViewCache->GetOrCreate(noiseTexture, sourceMip, 1));
        cr << SetShaderUniform(1, "OutNoise"_sh, RI.textureViewCache->GetOrCreate(noiseTexture, mip, 1));
        cr << SetShaderUniform(2, "CloudNoiseDownsampleConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << DispatchCompute(Vec3u { (targetExtent.x + 3) / 4, (targetExtent.y + 3) / 4, (targetExtent.z + 3) / 4 });
    }

    cr << InsertBarrier(noiseTexture->GetGpuImage(), ResourceState::ShaderResource);
}

const GpuImageViewRef& CloudPass::GetShapeNoiseView() const
{
    if (!m_noiseReady)
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_shapeNoise.Get());
}

const GpuImageViewRef& CloudPass::GetDetailNoiseView() const
{
    if (!m_noiseReady)
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_detailNoise.Get());
}

#pragma endregion Noise

#pragma region WeatherMap

bool CloudPass::NeedsFullWeatherMapRebuild(const CloudVolumeShaderData& cloudData, const Vec2f& origin) const
{
    if (!m_hasGeneratedWeatherMap)
    {
        return true;
    }

    if (origin != m_weatherMapShaderData.origin)
    {
        return true;
    }

    return cloudData.coverage != m_generatedCloudData.coverage
        || cloudData.cloudTypeBias != m_generatedCloudData.cloudTypeBias
        || cloudData.weatherScale != m_generatedCloudData.weatherScale
        || cloudData.cloudSize != m_generatedCloudData.cloudSize
        || cloudData.seed != m_generatedCloudData.seed;
}

void CloudPass::UpdateWeatherMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy)
{
    if (!m_weatherMap.IsValid())
    {
        m_weatherMap = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2DArray,
            TextureFormat::RGBA8,
            Vec3u { WeatherMapDimensions, WeatherMapDimensions, 1 },
            TextureFilterMode::LinearMipmap,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            NumWeatherKeyframes,
            ImageUsage::Storage | ImageUsage::Sampled
        });

        m_weatherMap->SetIsTransient(true);
        m_weatherMap->SetName(NAME("CloudWeatherMap"));

        Check(m_weatherMap->Create());
    }

    const CloudVolumeShaderData& cloudData = cloudVolumeProxy.bufferData.GetParams<CloudVolumeShaderData>();

    const double texelWorldSize = double(WeatherMapWorldExtent) / double(WeatherMapDimensions);
    const double snapWorldSize = texelWorldSize * double(WeatherMapOriginSnapTexels);
    const double halfWorldExtent = double(WeatherMapWorldExtent) * 0.5;

    // built in cloud space (world position plus the weather wind offset) so wind scrolls the clouds without a rebuild.
    // snapping keeps the content identical as the map follows the camera
    const double cloudSpaceCenterX = double(m_cameraPosition.x) + double(cloudData.weatherWindOffset.x);
    const double cloudSpaceCenterZ = double(m_cameraPosition.z) + double(cloudData.weatherWindOffset.y);

    const double originX = MathUtil::Floor<double, double>(cloudSpaceCenterX / snapWorldSize) * snapWorldSize - halfWorldExtent;
    const double originZ = MathUtil::Floor<double, double>(cloudSpaceCenterZ / snapWorldSize) * snapWorldSize - halfWorldExtent;

    const Vec2f origin = Vec2f(float(originX), float(originZ));

    const double evolutionTimePeriod = CloudEffectVolume::EvolutionTimePeriod;
    const auto wrapEvolutionTime = [evolutionTimePeriod](double evolutionTime)
    {
        return float(CloudEffectVolume::WrapToPeriod(evolutionTime, evolutionTimePeriod));
    };

    // signed, so a frame that reads a slightly older proxy holds the crossfade instead of looking like a wrap of the whole period
    float timeSincePreviousKeyframe = wrapEvolutionTime(double(cloudData.evolutionTime) - double(m_previousKeyframeEvolutionTime) + evolutionTimePeriod * 0.5)
        - float(evolutionTimePeriod * 0.5);

    const bool evolutionJumped = timeSincePreviousKeyframe >= 2.0f * WeatherKeyframeSeconds
        || timeSincePreviousKeyframe < -WeatherKeyframeSeconds;

    timeSincePreviousKeyframe = MathUtil::Max(timeSincePreviousKeyframe, 0.0f);

    bool generatedSlices = false;

    // a jump of more than a keyframe (first run, huge evolution speed) restarts the keyframes rather than crossfading across it
    if (NeedsFullWeatherMapRebuild(cloudData, origin) || evolutionJumped)
    {
        const double weatherNoisePeriodCells = double(CloudEffectVolume::WeatherNoisePeriodCells);

        // the weather wind offset wraps at the weather noise period, so the cloud lattice must repeat over that same distance.
        // kept even because the density noise runs at half the cloud lattice's frequency
        const double weatherPeriodWorldSize = double(cloudData.weatherScale) * weatherNoisePeriodCells;
        const double cellNoisePeriodCells = MathUtil::Max(MathUtil::Floor<double, double>(weatherPeriodWorldSize / double(cloudData.cloudSize) * 0.5 + 0.5) * 2.0, 2.0);
        const double cellWorldSize = weatherPeriodWorldSize / cellNoisePeriodCells;

        m_weatherMapShaderData.origin = origin;
        m_weatherMapShaderData.worldExtent = WeatherMapWorldExtent;
        m_weatherMapShaderData.dimensions = WeatherMapDimensions;

        m_weatherMapShaderData.noiseOrigin = Vec2f(
            float(CloudEffectVolume::WrapToPeriod(originX / double(cloudData.weatherScale), weatherNoisePeriodCells)),
            float(CloudEffectVolume::WrapToPeriod(originZ / double(cloudData.weatherScale), weatherNoisePeriodCells)));

        m_weatherMapShaderData.weatherNoisePeriodCells = CloudEffectVolume::WeatherNoisePeriodCells;
        m_weatherMapShaderData.evolutionNoisePeriodCells = CloudEffectVolume::EvolutionNoisePeriodCells;

        m_weatherMapShaderData.cellNoiseOrigin = Vec2f(
            float(CloudEffectVolume::WrapToPeriod(originX / cellWorldSize, cellNoisePeriodCells)),
            float(CloudEffectVolume::WrapToPeriod(originZ / cellWorldSize, cellNoisePeriodCells)));

        m_weatherMapShaderData.cellNoisePeriodCells = uint32(cellNoisePeriodCells);
        m_weatherMapShaderData.cellWorldSize = float(cellWorldSize);

        m_weatherMapShaderData.evolutionSecondsPerCell = float(CloudEffectVolume::EvolutionSecondsPerCell);

        // on a settings or placement change keep the current crossfade position, so only the content changes
        if (!m_hasGeneratedWeatherMap || evolutionJumped)
        {
            m_previousKeyframeEvolutionTime = cloudData.evolutionTime;
            timeSincePreviousKeyframe = 0.0f;
        }

        GenerateWeatherMapSlice(frame, cloudVolumeProxy, m_weatherMapShaderData.previousKeyframeSlice, m_previousKeyframeEvolutionTime);
        GenerateWeatherMapSlice(frame, cloudVolumeProxy, m_weatherMapShaderData.nextKeyframeSlice, wrapEvolutionTime(double(m_previousKeyframeEvolutionTime) + WeatherKeyframeSeconds));

        m_generatedCloudData = cloudData;
        m_hasGeneratedWeatherMap = true;

        // shadows are marched through this weather, so they follow the rebuild instead of catching up a band at a time
        m_shadowMapShaderData.isValid = 0;

        generatedSlices = true;
    }
    else if (timeSincePreviousKeyframe >= WeatherKeyframeSeconds)
    {
        // the next keyframe becomes the previous one, and the old previous slice is reused for the keyframe after it
        const uint32 reusedSlice = m_weatherMapShaderData.previousKeyframeSlice;

        m_weatherMapShaderData.previousKeyframeSlice = m_weatherMapShaderData.nextKeyframeSlice;
        m_weatherMapShaderData.nextKeyframeSlice = reusedSlice;

        m_previousKeyframeEvolutionTime = wrapEvolutionTime(double(m_previousKeyframeEvolutionTime) + WeatherKeyframeSeconds);
        timeSincePreviousKeyframe -= WeatherKeyframeSeconds;

        GenerateWeatherMapSlice(frame, cloudVolumeProxy, m_weatherMapShaderData.nextKeyframeSlice, wrapEvolutionTime(double(m_previousKeyframeEvolutionTime) + WeatherKeyframeSeconds));

        generatedSlices = true;
    }

    if (generatedSlices)
    {
        // shadow softness is a mip bias, so the chain has to follow every rebuild
        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_weatherMap->GetGpuImage(), ResourceState::CopyDst);
        cr << GenerateMipmaps(m_weatherMap.Get());
        cr << InsertBarrier(m_weatherMap->GetGpuImage(), ResourceState::ShaderResource);
    }

    m_weatherMapShaderData.keyframeBlend = MathUtil::Clamp(timeSincePreviousKeyframe / WeatherKeyframeSeconds, 0.0f, 1.0f);
}

void CloudPass::GenerateWeatherMapSlice(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 slice, float evolutionTime)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudWeatherMap);

    CloudWeatherMapShaderData generateShaderData = m_weatherMapShaderData;
    generateShaderData.generateSlice = slice;
    generateShaderData.generateEvolutionTime = evolutionTime;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cloudVolumeProxy.bufferData);
    RI.cbufferAllocator->Write(&generateShaderData);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_weatherMap->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudWeatherMap")));

    cr << SetShaderUniform(0, "OutWeatherMap"_sh, RI.textureViewCache->GetOrCreate(m_weatherMap.Get(), 0, 1));
    cr << SetShaderUniform(1, "CloudWeatherMapConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (WeatherMapDimensions + 7) / 8, (WeatherMapDimensions + 7) / 8, 1 });
}

const GpuImageViewRef& CloudPass::GetWeatherMapView() const
{
    if (!m_isActive)
    {
        return RI.placeholderData->GetImageView2D1x1R8Array();
    }

    return RI.textureViewCache->GetOrCreate(m_weatherMap.Get());
}

#pragma endregion WeatherMap

#pragma region ShadowMap

void CloudPass::UpdateShadowMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, const Vec3f& directionToSun)
{
    // marching placeholder noise would bake flat shadows that linger until the rows come around again
    if (!m_isActive || !m_noiseReady || directionToSun.LengthSquared() < MathUtil::epsilonF)
    {
        m_shadowMapShaderData.isValid = 0;

        return;
    }

    if (!m_shadowMap.IsValid())
    {
        m_shadowMap = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            TextureFormat::R16F,
            Vec3u { ShadowMapDimensions, ShadowMapDimensions, 1 },
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled
        });

        m_shadowMap->SetIsTransient(true);
        m_shadowMap->SetName(NAME("CloudShadowMap"));

        Check(m_shadowMap->Create());
    }

    const Vec3f normalizedDirectionToSun = directionToSun.Normalized();

    const double texelWorldSize = double(ShadowMapWorldExtent) / double(ShadowMapDimensions);
    const double snapWorldSize = texelWorldSize * double(ShadowMapOriginSnapTexels);
    const double halfWorldExtent = double(ShadowMapWorldExtent) * 0.5;

    // centered where the sun ray through the camera meets y = 0, since that's where nearby ground lands in the map
    const double lightHeight = MathUtil::Max(double(normalizedDirectionToSun.y), 0.05);
    const double centerX = double(m_cameraPosition.x) - double(normalizedDirectionToSun.x) * (double(m_cameraPosition.y) / lightHeight);
    const double centerZ = double(m_cameraPosition.z) - double(normalizedDirectionToSun.z) * (double(m_cameraPosition.y) / lightHeight);

    const Vec2f origin = Vec2f(
        float(MathUtil::Floor<double, double>(centerX / snapWorldSize) * snapWorldSize - halfWorldExtent),
        float(MathUtil::Floor<double, double>(centerZ / snapWorldSize) * snapWorldSize - halfWorldExtent));

    const bool needsFullRebuild = !m_shadowMapShaderData.isValid
        || origin != m_shadowMapShaderData.origin
        || normalizedDirectionToSun.Dot(m_shadowMapShaderData.directionToSun.GetXYZ()) < ShadowMapSunRebuildDot;

    if (needsFullRebuild)
    {
        m_shadowMapShaderData.origin = origin;
        m_shadowMapShaderData.worldExtent = ShadowMapWorldExtent;
        m_shadowMapShaderData.dimensions = ShadowMapDimensions;
        m_shadowMapShaderData.directionToSun = Vec4f(normalizedDirectionToSun, 0.0f);

        GenerateShadowMapRows(frame, cloudVolumeProxy, 0, ShadowMapDimensions);

        m_shadowMapNextRow = 0;
        m_shadowMapShaderData.isValid = 1;

        return;
    }

    const uint32 rowCount = MathUtil::Min(ShadowMapRowsPerFrame, ShadowMapDimensions - m_shadowMapNextRow);

    GenerateShadowMapRows(frame, cloudVolumeProxy, m_shadowMapNextRow, rowCount);

    m_shadowMapNextRow = (m_shadowMapNextRow + rowCount) % ShadowMapDimensions;
}

void CloudPass::GenerateShadowMapRows(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 rowStart, uint32 rowCount)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudShadowMap);

    CloudShadowMapShaderData generateShaderData = m_shadowMapShaderData;
    generateShaderData.generateRowStart = rowStart;
    generateShaderData.generateRowCount = rowCount;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cloudVolumeProxy.bufferData);
    RI.cbufferAllocator->Write(&m_weatherMapShaderData);
    RI.cbufferAllocator->Write(&generateShaderData);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_weatherMap->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(m_shadowMap->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudShadowMap")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "OutShadowMap"_sh, RI.textureViewCache->GetOrCreate(m_shadowMap.Get()));
    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, GetWeatherMapView());
    cr << SetShaderUniform(uniformIndex++, "CloudShapeNoiseTexture"_sh, GetShapeNoiseView());
    cr << SetShaderUniform(uniformIndex++, "CloudDetailNoiseTexture"_sh, GetDetailNoiseView());
    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "CloudShadowMapConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (ShadowMapDimensions + 7) / 8, (rowCount + 7) / 8, 1 });

    cr << InsertBarrier(m_shadowMap->GetGpuImage(), ResourceState::ShaderResource);
}

const GpuImageViewRef& CloudPass::GetShadowMapView() const
{
    if (!m_isActive || !m_shadowMapShaderData.isValid)
    {
        return RI.placeholderData->GetImageView2D1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_shadowMap.Get());
}

#pragma endregion ShadowMap

#pragma region SkyProbe

void CloudPass::CompositeSkyProbe(Texture* skyTexture, Texture* skyProbeTexture, const EnvProbeShaderData& skyProbeShaderData, const LightShaderData* sunShaderData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(skyTexture != nullptr && skyProbeTexture != nullptr);

    if (!CanCompositeSkyProbe())
    {
        return;
    }

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });

    ENGINE_STAT_GPU_SCOPE(&s_statCloudSkyProbe, &cr);

    const uint32 dimensions = skyProbeTexture->GetExtent().x;

    CloudSkyProbeConstants constants {};
    constants.rayOrigin = Vec4f(m_cameraPosition, 1.0f);
    constants.dimensions = dimensions;
    constants.traceSteps = SkyProbeTraceSteps;
    constants.lightSteps = SkyProbeLightSteps;
    constants.hasSun = sunShaderData ? 1u : 0u;

    const LightShaderData sun = sunShaderData ? *sunShaderData : LightShaderData {};

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&sun);
    RI.cbufferAllocator->Write(&skyProbeShaderData);
    WriteShaderData(*RI.cbufferAllocator);
    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    const ImageSubResource facesSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 6 };

    cr << InsertBarrier(skyTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(skyProbeTexture->GetGpuImage(), ResourceState::UnorderedAccess, facesSubResource);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudSkyProbe")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "OutSkyProbeTexture"_sh, RI.textureViewCache->GetOrCreate(skyProbeTexture, facesSubResource, TextureType::Texture2DArray));
    cr << SetShaderUniform(uniformIndex++, "InSkyTexture"_sh, RI.textureViewCache->GetOrCreate(skyTexture));
    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, GetWeatherMapView());
    cr << SetShaderUniform(uniformIndex++, "CloudShapeNoiseTexture"_sh, GetShapeNoiseView());
    cr << SetShaderUniform(uniformIndex++, "CloudDetailNoiseTexture"_sh, GetDetailNoiseView());
    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "CloudSkyProbeConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (dimensions + 7) / 8, (dimensions + 7) / 8, 6 });

    cr << InsertBarrier(skyProbeTexture->GetGpuImage(), ResourceState::ShaderResource);
}

#pragma endregion SkyProbe

#pragma region Sky

void CloudPass::CreateTraceTextures()
{
    const auto createCloudTexture = [](Handle<Texture>& texture, const Vec2u& textureExtent, TextureFormat format, Name name)
    {
        if (texture.IsValid())
        {
            EnqueueDeletion(std::move(texture));
        }

        texture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            format,
            Vec3u { textureExtent.x, textureExtent.y, 1 },
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled
        });

        texture->SetIsTransient(true);
        texture->SetName(name);

        Check(texture->Create());
    };

    m_historyExtent = Vec2u(MathUtil::Max((m_extent.x + 1) / 2, 1u), MathUtil::Max((m_extent.y + 1) / 2, 1u));
    m_traceExtent = Vec2u(MathUtil::Max((m_historyExtent.x + 1) / 2, 1u), MathUtil::Max((m_historyExtent.y + 1) / 2, 1u));

    createCloudTexture(m_traceTexture, m_traceExtent, TextureFormat::RGBA16F, NAME("CloudTraceTexture"));
    createCloudTexture(m_traceDistanceTexture, m_traceExtent, TextureFormat::R32F, NAME("CloudTraceDistanceTexture"));

    for (uint32 historySlot = 0; historySlot < 2; historySlot++)
    {
        createCloudTexture(m_historyTextures[historySlot], m_historyExtent, TextureFormat::RGBA16F, NAME_FMT("CloudHistoryTexture{}", historySlot));
        createCloudTexture(m_historyDistanceTextures[historySlot], m_historyExtent, TextureFormat::R32F, NAME_FMT("CloudHistoryDistanceTexture{}", historySlot));
    }

    // the old history is the wrong size to reproject from
    m_lastRenderedFrame = ~0u;
}

void CloudPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view != nullptr);
    AssertDebug(renderSetup.framebuffer != nullptr);

    // the noise fills in over the first frames clouds are active; tracing against placeholders would flash
    if (!m_isActive || !m_noiseReady)
    {
        return;
    }

    if (!Trace(frame, renderSetup))
    {
        return;
    }

    Composite(frame, renderSetup);
}

bool CloudPass::Trace(Frame* frame, const RenderSetup& renderSetup)
{
    DeferredPassData* deferredPassData = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(deferredPassData != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return false;
    }

    const uint32 frameCounter = GetFrameCounter();
    const Vec2u traceOffset = TraceOffsetSequence[frameCounter % 4];

    const Vec3f cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();
    const Vec3f cameraDirection = cameraProxy->bufferData.cameraDirection.GetXYZ();

    // history is only reprojectable from the frame directly before, taken from a similar viewpoint
    const bool isCameraCut = cameraDirection.Dot(m_previousCameraDirection) < CameraCutMinDirectionDot
        || cameraPosition.Distance(m_previousCameraPosition) > CameraCutMaxMoveDistance;

    const bool isHistoryValid = m_lastRenderedFrame != ~0u
        && frameCounter == m_lastRenderedFrame + 1
        && !isCameraCut;

    ENGINE_STAT_GPU_SCOPE(&s_statCloudTrace);

    LightShaderData sunShaderData {};
    bool hasSun = false;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() != LightType::Directional)
        {
            continue;
        }

        if (RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light)))
        {
            sunShaderData = lightProxy->bufferData;
            hasSun = true;

            break;
        }
    }

    EnvProbeShaderData skyProbeShaderData {};

    // haze blends toward the sky probe's cloud-free capture - the probe's convolved maps have the clouds themselves in them
    GpuImageViewRef skyTextureView = RI.placeholderData->GetImageViewCube1x1R8();

    const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();

    if (skyProbes.Any())
    {
        SkyProbe* skyProbe = static_cast<SkyProbe*>(*skyProbes.Begin());

        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(skyProbe)))
        {
            skyProbeShaderData = skyProbeProxy->bufferData;
        }

        const Handle<Texture>& skyboxTexture = skyProbe->GetSkyboxCubemap();

        if (skyboxTexture.IsValid() && skyboxTexture->IsCreated())
        {
            skyTextureView = RI.textureViewCache->GetOrCreate(skyboxTexture.Get());
        }
    }

    CloudTraceConstants traceConstants {};
    traceConstants.traceDimensions = m_traceExtent;
    traceConstants.traceSteps = MathUtil::Max(g_cvCloudsTraceSteps.Get(), 1u);
    traceConstants.lightSteps = g_cvCloudsLightSteps.Get();
    traceConstants.frameCounter = frameCounter;
    traceConstants.hasSun = hasSun ? 1u : 0u;
    traceConstants.traceOffset = traceOffset;
    traceConstants.historyDimensions = m_historyExtent;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&sunShaderData);
    RI.cbufferAllocator->Write(&skyProbeShaderData);
    WriteShaderData(*RI.cbufferAllocator);
    RI.cbufferAllocator->Write(&traceConstants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    // Hi-Z mip 2: a texel covers the same 4x4 full resolution block as a trace texel
    const uint32 depthPyramidMip = MathUtil::Min(2u, deferredPassData->depthPyramidRenderer->GetTotalMips() - 1);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_traceTexture->GetGpuImage(), ResourceState::UnorderedAccess);
    cr << InsertBarrier(m_traceDistanceTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudTrace")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "OutCloudTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "OutCloudDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceDistanceTexture.Get()));

    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, GetWeatherMapView());
    cr << SetShaderUniform(uniformIndex++, "CloudShapeNoiseTexture"_sh, GetShapeNoiseView());
    cr << SetShaderUniform(uniformIndex++, "CloudDetailNoiseTexture"_sh, GetDetailNoiseView());

    cr << SetShaderUniform(uniformIndex++, "DepthPyramidTexture"_sh, RI.textureViewCache->GetOrCreate(deferredPassData->depthPyramidRenderer->GetHZBTexture(), depthPyramidMip, 1));

    cr << SetShaderUniform(uniformIndex++, "SkyTexture"_sh, skyTextureView);
    cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    cr << SetShaderUniform(uniformIndex++, "CloudTraceConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_traceExtent.x + 7) / 8, (m_traceExtent.y + 7) / 8, 1 });

    cr << InsertBarrier(m_traceTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(m_traceDistanceTexture->GetGpuImage(), ResourceState::ShaderResource);

    Reconstruct(frame, *cameraProxy, isHistoryValid, traceOffset);

    m_previousViewProjection = cameraProxy->bufferData.viewProjMat;
    m_previousCameraPosition = cameraPosition;
    m_previousCameraDirection = cameraDirection;

    m_lastRenderedFrame = frameCounter;

    return true;
}

void CloudPass::Reconstruct(Frame* frame, const RenderProxyCamera& cameraProxy, bool isHistoryValid, const Vec2u& traceOffset)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudReconstruct);

    const uint32 previousHistoryIndex = m_historyIndex;
    const uint32 nextHistoryIndex = m_historyIndex ^ 1u;

    CloudReconstructConstants reconstructConstants {};
    reconstructConstants.previousViewProjection = m_previousViewProjection;
    reconstructConstants.traceDimensions = m_traceExtent;
    reconstructConstants.historyDimensions = m_historyExtent;
    reconstructConstants.traceOffset = traceOffset;
    reconstructConstants.historyValid = isHistoryValid ? 1u : 0u;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy.bufferData);
    RI.cbufferAllocator->Write(&reconstructConstants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    // clamped so reprojected lookups at the screen edge don't wrap to the opposite side
    Sampler* clampedLinearSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    });

    Texture* outHistoryTexture = m_historyTextures[nextHistoryIndex].Get();
    Texture* outHistoryDistanceTexture = m_historyDistanceTextures[nextHistoryIndex].Get();

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::UnorderedAccess);
    cr << InsertBarrier(outHistoryDistanceTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudReconstruct")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "InTraceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "InTraceDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceDistanceTexture.Get()));

    cr << SetShaderUniform(uniformIndex++, "InHistoryTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[previousHistoryIndex].Get()));
    cr << SetShaderUniform(uniformIndex++, "InHistoryDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyDistanceTextures[previousHistoryIndex].Get()));

    cr << SetShaderUniform(uniformIndex++, "OutHistoryTexture"_sh, RI.textureViewCache->GetOrCreate(outHistoryTexture));
    cr << SetShaderUniform(uniformIndex++, "OutHistoryDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(outHistoryDistanceTexture));

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, clampedLinearSampler);

    cr << SetShaderUniform(uniformIndex++, "CloudReconstructConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_historyExtent.x + 7) / 8, (m_historyExtent.y + 7) / 8, 1 });

    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(outHistoryDistanceTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_historyIndex = nextHistoryIndex;
}

void CloudPass::Composite(Frame* frame, const RenderSetup& renderSetup)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudComposite);

    // clamped so the half resolution texture doesn't wrap its opposite edge into the screen border
    Sampler* compositeSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    });

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentFramebuffer(renderSetup.framebuffer);
    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    // sky pixels only - geometry is never behind a ground level camera's clouds
    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
    cr << SetStencilState(SkyStencilMask, SkyStencilMask, 0x0);

    // premultiplied: color = cloud + sky * transmittance, destination alpha untouched
    cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::SrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));

    cr << SetCurrentShader(ShaderDesc(NAME("CloudComposite")));

    cr << SetShaderUniform(0, "SamplerLinear"_sh, compositeSampler);
    cr << SetShaderUniform(1, "InCloudTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[m_historyIndex].Get()));

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));

    cr << DrawIndexed(6);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentFramebuffer(nullptr);
}

#pragma endregion Sky

} // namespace Hyperion
