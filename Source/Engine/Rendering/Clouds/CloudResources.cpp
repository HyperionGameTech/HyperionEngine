/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Clouds/CloudResources.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Sky/CloudEffectVolume.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

extern uint32 GetFrameCounter();

CVar<bool> g_cvClouds { "Rendering.Clouds", true };
CVar<bool> g_cvCloudsDebugShadows { "Rendering.Clouds.DebugShadows", false };

// seconds between sky probe recaptures while clouds are active, so ambient and reflections follow them
CVar<float> g_cvCloudsSkyProbeRefreshSeconds { "Rendering.Clouds.SkyProbeRefreshSeconds", 2.0f };

static EngineStatGpuTimer s_statCloudWeatherMap("Rendering/GPU/CloudWeatherMap");
static EngineStatGpuTimer s_statCloudNoise("Rendering/GPU/CloudNoiseGenerate");
static EngineStatGpuTimer s_statCloudShadowMap("Rendering/GPU/CloudShadowMap");
static EngineStatGpuTimer s_statCloudSkyProbe("Rendering/GPU/CloudSkyProbe");

static StaticShaderPropertyId s_propNoiseModeShape { ShaderProperty(NAME("MODE"), NAME("SHAPE")) };
static StaticShaderPropertyId s_propNoiseModeDetail { ShaderProperty(NAME("MODE"), NAME("DETAIL")) };

// matches CloudNoiseConstants in Shaders/Clouds/CloudNoiseGenerate.hlsl
struct CloudNoiseConstants
{
    uint32 dimensions;
    uint32 sliceStart;
    uint32 sliceCount;
    uint32 _pad0;
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

// matches CloudNoiseDownsampleConstants in Shaders/Clouds/CloudNoiseDownsample.hlsl
struct CloudNoiseDownsampleConstants
{
    Vec4u sourceDimensions;
    Vec4u targetDimensions;
};

CloudResources::CloudResources()
    : m_shadowMapShaderData {},
      m_shadowMapNextRow(0),
      m_shapeNoiseSlicesGenerated(0),
      m_noiseReady(false),
      m_weatherMapShaderData {},
      m_generatedCloudData {},
      m_previousKeyframeEvolutionTime(0.0f),
      m_lastActiveFrame(0),
      m_cloudVolumeShaderData {},
      m_cameraPosition(Vec3f::Zero()),
      m_hasGeneratedWeatherMap(false),
      m_isActive(false)
{
    m_weatherMapShaderData.previousKeyframeSlice = 0;
    m_weatherMapShaderData.nextKeyframeSlice = 1;
}

CloudResources::~CloudResources()
{
    Shutdown();
}

void CloudResources::Shutdown()
{
    if (m_weatherMap.IsValid())
    {
        EnqueueDeletion(std::move(m_weatherMap));
    }

    if (m_shadowMap.IsValid())
    {
        EnqueueDeletion(std::move(m_shadowMap));
    }

    m_shadowMapShaderData.isValid = 0;
    m_shadowMapNextRow = 0;

    if (m_shapeNoise.IsValid())
    {
        EnqueueDeletion(std::move(m_shapeNoise));
    }

    if (m_detailNoise.IsValid())
    {
        EnqueueDeletion(std::move(m_detailNoise));
    }

    m_shapeNoiseSlicesGenerated = 0;
    m_noiseReady = false;

    m_hasGeneratedWeatherMap = false;
    m_isActive = false;
}

void CloudResources::CreateNoiseTextures()
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
            ImageUsage::Storage | ImageUsage::Sampled });

        noiseTexture->SetIsTransient(true);
        noiseTexture->SetName(name);

        Check(noiseTexture->Create());

        return noiseTexture;
    };

    m_shapeNoise = createNoiseTexture(ShapeNoiseDimensions, NAME("CloudShapeNoise"));
    m_detailNoise = createNoiseTexture(DetailNoiseDimensions, NAME("CloudDetailNoise"));
}

void CloudResources::UpdateNoise(Frame* frame)
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

void CloudResources::GenerateNoiseSlices(Frame* frame, Texture* noiseTexture, bool isShapeNoise, uint32 sliceStart, uint32 sliceCount)
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

void CloudResources::GenerateNoiseMips(Frame* frame, Texture* noiseTexture)
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

const GpuImageViewRef& CloudResources::GetShapeNoiseView() const
{
    if (!m_noiseReady)
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_shapeNoise.Get());
}

const GpuImageViewRef& CloudResources::GetDetailNoiseView() const
{
    if (!m_noiseReady)
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_detailNoise.Get());
}

void CloudResources::CreateWeatherMap()
{
    m_weatherMap = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::RGBA8,
        Vec3u { WeatherMapDimensions, WeatherMapDimensions, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge,
        NumWeatherKeyframes,
        ImageUsage::Storage | ImageUsage::Sampled });

    m_weatherMap->SetIsTransient(true);
    m_weatherMap->SetName(NAME("CloudWeatherMap"));

    Check(m_weatherMap->Create());
}

bool CloudResources::NeedsFullWeatherMapRebuild(const CloudVolumeShaderData& cloudData, const Vec2f& origin) const
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

void CloudResources::Update(Frame* frame, const RenderProxyEffectVolume* cloudVolumeProxy, const Vec3f& cameraPosition, const Vec3f& directionToSun)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_isActive = false;

    m_weatherMapShaderData.debugShadows = g_cvCloudsDebugShadows.Get() ? 1u : 0u;

    if (!g_cvClouds.Get() || !cloudVolumeProxy || !cloudVolumeProxy->bufferData.GetParams<CloudVolumeShaderData>().enabled)
    {
        // the map stops following wind and evolution while inactive, so it's rebuilt from scratch when clouds come back
        m_shadowMapShaderData.isValid = 0;

        return;
    }

    const CloudVolumeShaderData cloudData = cloudVolumeProxy->bufferData.GetParams<CloudVolumeShaderData>();

    m_cloudVolumeShaderData = cloudVolumeProxy->bufferData;
    m_cameraPosition = cameraPosition;

    UpdateNoise(frame);

    if (!m_weatherMap.IsValid())
    {
        CreateWeatherMap();
    }

    const double texelWorldSize = double(WeatherMapWorldExtent) / double(WeatherMapDimensions);
    const double snapWorldSize = texelWorldSize * double(WeatherMapOriginSnapTexels);
    const double halfWorldExtent = double(WeatherMapWorldExtent) * 0.5;

    // built in cloud space (world position plus the weather wind offset) so wind scrolls the clouds without a rebuild.
    // snapping keeps the content identical as the map follows the camera
    const double cloudSpaceCenterX = double(cameraPosition.x) + double(cloudData.weatherWindOffset.x);
    const double cloudSpaceCenterZ = double(cameraPosition.z) + double(cloudData.weatherWindOffset.y);

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

        GenerateWeatherMapSlice(frame, *cloudVolumeProxy, m_weatherMapShaderData.previousKeyframeSlice, m_previousKeyframeEvolutionTime);
        GenerateWeatherMapSlice(frame, *cloudVolumeProxy, m_weatherMapShaderData.nextKeyframeSlice, wrapEvolutionTime(double(m_previousKeyframeEvolutionTime) + WeatherKeyframeSeconds));

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

        GenerateWeatherMapSlice(frame, *cloudVolumeProxy, m_weatherMapShaderData.nextKeyframeSlice, wrapEvolutionTime(double(m_previousKeyframeEvolutionTime) + WeatherKeyframeSeconds));

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

    m_isActive = m_hasGeneratedWeatherMap;

    UpdateShadowMap(frame, *cloudVolumeProxy, cameraPosition, directionToSun);
}

void CloudResources::CreateShadowMap()
{
    m_shadowMap = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::R16F,
        Vec3u { ShadowMapDimensions, ShadowMapDimensions, 1 },
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge,
        1,
        ImageUsage::Storage | ImageUsage::Sampled });

    m_shadowMap->SetIsTransient(true);
    m_shadowMap->SetName(NAME("CloudShadowMap"));

    Check(m_shadowMap->Create());
}

void CloudResources::UpdateShadowMap(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, const Vec3f& cameraPosition, const Vec3f& directionToSun)
{
    // marching placeholder noise would bake flat shadows that linger until the rows come around again
    if (!m_isActive || !m_noiseReady || directionToSun.LengthSquared() < MathUtil::epsilonF)
    {
        m_shadowMapShaderData.isValid = 0;

        return;
    }

    if (!m_shadowMap.IsValid())
    {
        CreateShadowMap();
    }

    const Vec3f normalizedDirectionToSun = directionToSun.Normalized();

    const double texelWorldSize = double(ShadowMapWorldExtent) / double(ShadowMapDimensions);
    const double snapWorldSize = texelWorldSize * double(ShadowMapOriginSnapTexels);
    const double halfWorldExtent = double(ShadowMapWorldExtent) * 0.5;

    // centered where the sun ray through the camera meets y = 0, since that's where nearby ground lands in the map
    const double lightHeight = MathUtil::Max(double(normalizedDirectionToSun.y), 0.05);
    const double centerX = double(cameraPosition.x) - double(normalizedDirectionToSun.x) * (double(cameraPosition.y) / lightHeight);
    const double centerZ = double(cameraPosition.z) - double(normalizedDirectionToSun.z) * (double(cameraPosition.y) / lightHeight);

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

void CloudResources::GenerateShadowMapRows(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 rowStart, uint32 rowCount)
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

const GpuImageViewRef& CloudResources::GetShadowMapView() const
{
    if (!m_isActive || !m_shadowMapShaderData.isValid)
    {
        return RI.placeholderData->GetImageView2D1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_shadowMap.Get());
}

void CloudResources::GenerateWeatherMapSlice(Frame* frame, const RenderProxyEffectVolume& cloudVolumeProxy, uint32 slice, float evolutionTime)
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

const GpuImageViewRef& CloudResources::GetWeatherMapView() const
{
    if (!m_isActive)
    {
        return RI.placeholderData->GetImageView2D1x1R8Array();
    }

    return RI.textureViewCache->GetOrCreate(m_weatherMap.Get());
}

void CloudResources::WriteShaderData(CBufferAllocator& cbufferAllocator, RenderProxyList& rpl) const
{
    const RenderProxyEffectVolume* cloudVolumeProxy = nullptr;

    if (m_isActive)
    {
        for (EffectVolume* effectVolume : rpl.GetEffectVolumes().GetElements<CloudEffectVolume>())
        {
            cloudVolumeProxy = static_cast<RenderProxyEffectVolume*>(GetRenderProxy(effectVolume));

            if (cloudVolumeProxy)
            {
                break;
            }
        }
    }

    if (cloudVolumeProxy)
    {
        cbufferAllocator.Write(&cloudVolumeProxy->bufferData);
    }
    else
    {
        // zeroed params read as disabled, so shaders skip clouds
        const EffectVolumeShaderData disabledCloudVolume {};
        cbufferAllocator.Write(&disabledCloudVolume);
    }

    cbufferAllocator.Write(&m_weatherMapShaderData);

    CloudShadowMapShaderData shadowMapShaderData = m_shadowMapShaderData;
    shadowMapShaderData.isValid = (cloudVolumeProxy && m_shadowMapShaderData.isValid) ? 1u : 0u;

    cbufferAllocator.Write(&shadowMapShaderData);
}

void CloudResources::CompositeSkyProbe(Texture* skyTexture, Texture* skyProbeTexture, const EnvProbeShaderData& skyProbeShaderData, const LightShaderData* sunShaderData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(skyTexture != nullptr && skyProbeTexture != nullptr);

    if (!m_isActive || !m_noiseReady)
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
    RI.cbufferAllocator->Write(&m_cloudVolumeShaderData);
    RI.cbufferAllocator->Write(&m_weatherMapShaderData);
    RI.cbufferAllocator->Write(&m_shadowMapShaderData);
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

} // namespace Hyperion
