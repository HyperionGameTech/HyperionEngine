/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerMath.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/EnvProbe.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerProbes("Rendering/GPU/Glimmer/Probes");

static StaticShaderPropertyId s_propTraceModeTrace { ShaderProperty(NAME("MODE"), NAME("TRACE")) };
static StaticShaderPropertyId s_propTraceModeShade { ShaderProperty(NAME("MODE"), NAME("SHADE")) };

// Must match GlimmerProbeRayHit in Shaders/Glimmer/SWRT/GlimmerSWRTProbeTrace.hlsl
static constexpr size_t RayHitSize = 4 * sizeof(Vec4f);

// Must match PROBES_PER_GROUP in Shaders/Glimmer/SWRT/GlimmerSWRTProbeTrace.hlsl: a lane per ray, filling a wave of 32
static constexpr uint32 ProbesPerTraceGroup = 32 / GlimmerProbeRays;

static_assert(GlimmerProbesPerCascade % ProbesPerTraceGroup == 0);
// slices are made of 2x2 column tiles, scrambled as an 8 bit index (GlimmerProbeTileOrder in GlimmerProbeTypes.hlsli)
static_assert(GlimmerProbeGrid == 32, "slices scramble 16 x 16 tiles");
static_assert((2u << (GlimmerProbeCascades - 1)) * 4u <= 256u, "every slice needs at least one tile");

static constexpr uint32 BaseGroupSize = 64;
static constexpr uint32 BlendGroupSize = 64;

// per frame, so a cascade updated every n frames blends by these to the n: steady light settles over ~4 s at 30 fps, while a
// probe whose light jumps (2x or more) catches up over ~1 s
static constexpr float ProbesHysteresisSteady = 0.995f;
static constexpr float ProbesHysteresisChanging = 0.965f;

// escaping rays brighter than this are scaled down, so a sliver of sun in the sky probe doesn't make fireflies
static constexpr float ProbesEscapeClamp = 64.0f;

static constexpr float ProbesMaxDistance = 2000.0f;

// how many probe spacings SWRT traces a near field probe's rays before the heightfield takes over
static constexpr float NearFieldReachSpacings = 8.0f;

// Must match GlimmerProbeBaseConstants in Shaders/Glimmer/SWRT/GlimmerSWRTProbeBase.hlsl
struct GlimmerProbeBaseConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
};

// Must match GlimmerProbeTraceConstants in Shaders/Glimmer/SWRT/GlimmerSWRTProbeTrace.hlsl
struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    Vec4u dispatch; // y = sky probe color texture index (~0 without one)
    Vec4f sky;      // x = sky probe diffuse strength, y = foliage extinction
    Vec4u cascadeDispatches[GlimmerProbeCascades]; // x = GlimmerProbeDispatchMode, y = slice period | slice << 16, z = probes
};

// Must match GlimmerProbeBlendConstants in Shaders/Glimmer/SWRT/GlimmerSWRTProbeBlend.hlsl
struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolumeShaderData volume;
    Vec4u cascadeDispatches[GlimmerProbeCascades]; // x = GlimmerProbeDispatchMode, y = slice period | slice << 16, z = probes
};

static float GetCascadeSpacing(uint32 cascadeIndex)
{
    return 2.0f * float(1u << cascadeIndex);
}

static uint32 GetCascadeUpdatePeriod(uint32 cascadeIndex)
{
    return 2u << MathUtil::Max(cascadeIndex, 1u);
}

// Must match GLIMMER_PROBE_DISPATCH_* in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
enum GlimmerProbeDispatchMode : uint32
{
    GPDM_ALL = 0,
    GPDM_SCROLLED = 1,
    GPDM_SLICE = 2
};

static Handle<Texture> CreateProbeTexture(TextureType type, TextureFormat format, const Vec3u& extent, uint16 numLayers, Name name)
{
    Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
        type,
        format,
        extent,
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::Repeat,
        numLayers,
        ImageUsage::Storage | ImageUsage::Sampled });

    texture->SetIsTransient(true);
    texture->SetName(name);
    Check(texture->Create());

    return texture;
}

GlimmerSWRTProbeVolume::GlimmerSWRTProbeVolume()
    : m_hasGridOrigins(false),
      m_frameIndex(0),
      m_shaderData {}
{
}

GlimmerSWRTProbeVolume::~GlimmerSWRTProbeVolume()
{
    EnqueueDeletion(std::move(m_raysBuffer));
    EnqueueDeletion(std::move(m_rayHitsBuffer));
}

void GlimmerSWRTProbeVolume::CreateResources()
{
    const Vec3u probeExtent = Vec3u(GlimmerProbeGrid, GlimmerProbeCascades * GlimmerProbeLayers, GlimmerProbeGrid);

    m_shTextures[0] = CreateProbeTexture(TextureType::Texture3D, TextureFormat::RGBA16F, probeExtent, 1, NAME("GlimmerProbeSH0"));
    m_shTextures[1] = CreateProbeTexture(TextureType::Texture3D, TextureFormat::RGBA16F, probeExtent, 1, NAME("GlimmerProbeSH1"));
    m_shTextures[2] = CreateProbeTexture(TextureType::Texture3D, TextureFormat::RGBA16F, probeExtent, 1, NAME("GlimmerProbeSH2"));
    m_stateTexture = CreateProbeTexture(TextureType::Texture3D, TextureFormat::RG32, probeExtent, 1, NAME("GlimmerProbeState"));
    m_trendTexture = CreateProbeTexture(TextureType::Texture3D, TextureFormat::R16F, probeExtent, 1, NAME("GlimmerProbeTrend"));
    m_baseTexture = CreateProbeTexture(TextureType::Texture2DArray, TextureFormat::R32F, Vec3u(GlimmerProbeGrid, GlimmerProbeGrid, 1), uint16(GlimmerProbeCascades), NAME("GlimmerSWRTProbeBase"));

    m_raysBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, size_t(GlimmerProbeCascades) * GlimmerProbesPerCascade * GlimmerProbeRays * sizeof(Vec4f), alignof(Vec4f));
    Check(m_raysBuffer->Create());

    m_rayHitsBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, size_t(GlimmerProbeCascades) * GlimmerProbesPerCascade * GlimmerProbeRays * RayHitSize, alignof(Vec4f));
    Check(m_rayHitsBuffer->Create());
}

const GpuImageViewRef& GlimmerSWRTProbeVolume::GetSHImageView(uint32 channel) const
{
    if (!m_shTextures[channel].IsValid())
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_shTextures[channel]);
}

const GpuImageViewRef& GlimmerSWRTProbeVolume::GetStateImageView() const
{
    if (!m_stateTexture.IsValid())
    {
        return RI.placeholderData->GetImageView3D1x1x1R8();
    }

    return RI.textureViewCache->GetOrCreate(m_stateTexture);
}

const GpuImageViewRef& GlimmerSWRTProbeVolume::GetBaseImageView() const
{
    if (!m_baseTexture.IsValid())
    {
        return RI.placeholderData->GetImageView2D1x1R8Array();
    }

    return RI.textureViewCache->GetOrCreate(m_baseTexture);
}

void GlimmerSWRTProbeVolume::ScrollCascades(const Vec3f& viewerPosition, uint32& outScrolledMask)
{
    outScrolledMask = 0;

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerProbeCascades; cascadeIndex++)
    {
        const float spacing = GetCascadeSpacing(cascadeIndex);
        const int32 half = int32(GlimmerProbeGrid / 2);

        const Vec2i origin = Vec2i(
            int32(MathUtil::Floor(viewerPosition.x / spacing)) - half,
            int32(MathUtil::Floor(viewerPosition.z / spacing)) - half);

        if (!m_hasGridOrigins || origin != m_gridOrigins[cascadeIndex])
        {
            outScrolledMask |= 1u << cascadeIndex;
        }

        m_gridOrigins[cascadeIndex] = origin;
    }

    m_hasGridOrigins = true;
}

void GlimmerSWRTProbeVolume::Update(Frame* frame, const GlimmerSWRTProbeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.blasCache || !inputs.blasCache->IsReady() || !inputs.spanCache->GetSpansBuffer().IsValid())
    {
        return;
    }

    if (!m_raysBuffer.IsValid())
    {
        CreateResources();
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbes);

    m_frameIndex++;

    uint32 scrolledMask = 0;
    ScrollCascades(inputs.viewerPosition, scrolledMask);


    const bool hasSWRTScene = inputs.tlas && inputs.tlas->IsReady();

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerProbeCascades; cascadeIndex++)
    {
        GlimmerProbeCascadeShaderData& cascade = m_shaderData.cascades[cascadeIndex];

        // hysteresis is per update, so cascades updated less often blend faster and every cascade settles in about the same time
        const float period = float(GetCascadeUpdatePeriod(cascadeIndex));

        cascade.gridOrigin = Vec4i(m_gridOrigins[cascadeIndex].x, m_gridOrigins[cascadeIndex].y, cascade.gridOrigin.z, 0);
        cascade.params = Vec4f(GetCascadeSpacing(cascadeIndex), float(1u << cascadeIndex), MathUtil::Pow(ProbesHysteresisSteady, period), MathUtil::Pow(ProbesHysteresisChanging, period));
    }

    m_shaderData.info = Vec4u(GlimmerProbeCascades, GlimmerProbeRays, m_frameIndex, m_shaderData.info.w);
    m_shaderData.rayRotation = MakeGlimmerRandomRotation(m_frameIndex);
    m_shaderData.params = Vec4f(
        inputs.viewerPosition.y - 2.0f,
        0.0f,
        ProbesEscapeClamp,
        ProbesMaxDistance);
    m_shaderData.nearField = Vec4f(
        float(MathUtil::Clamp(g_cvGlimmerSWRTNearFieldCascades.Get(), 0, int(GlimmerProbeCascades))),
        NearFieldReachSpacings,
        hasSWRTScene ? float(inputs.tlas->GetNumInstances()) : 0.0f,
        MathUtil::Clamp(g_cvGlimmerGroundAlbedo.Get(), 0.0f, 1.0f));

    CommandRecorder& cr = frame->cr;

    const GlimmerGroundShaderData& groundShaderData = inputs.surfaceCache->GetGroundShaderData();
    const GpuImageViewRef& groundImageView = inputs.surfaceCache->GetGroundImageView();

    { // column bases follow the ground under each column
        GlimmerProbeBaseConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        cr << InsertBarrier(m_baseTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeBase")));
        cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(1, "GlimmerGroundTexture"_sh, groundImageView);
        cr << SetShaderUniform(2, "OutProbeBase"_sh, RI.textureViewCache->GetOrCreate(m_baseTexture));

        cr << DispatchCompute(Vec3u { (GlimmerProbeCascades * GlimmerProbeGrid * GlimmerProbeGrid + BaseGroupSize - 1) / BaseGroupSize, 1, 1 });

        cr << InsertBarrier(m_baseTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    uint32 skyTextureIndex = ~0u;
    float skyDiffuseStrength = 0.0f;

    if (inputs.skyProbe)
    {
        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(inputs.skyProbe)))
        {
            skyTextureIndex = skyProbeProxy->bufferData.textureIndices & 0xFFFFu;
            skyDiffuseStrength = skyProbeProxy->bufferData.worldPosition.w;
        }
    }

    const GpuBufferRef& tlasNodes = hasSWRTScene ? inputs.tlas->GetNodesBuffer() : inputs.blasCache->GetNodesBuffer();
    const GpuBufferRef& tlasInstances = hasSWRTScene ? inputs.tlas->GetInstancesBuffer() : inputs.blasCache->GetTrianglesBuffer();

    // every cascade in one dispatch per pass (the cascade is the group's y), as a slice is too small to fill the GPU on its own
    Vec4u cascadeDispatches[GlimmerProbeCascades];
    uint32 maxProbes = 0;

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerProbeCascades; cascadeIndex++)
    {
        // every frame updates one slice of each cascade's probes, so light changes creep in rather than stepping
        const uint32 period = GetCascadeUpdatePeriod(cascadeIndex);
        const uint32 slice = m_frameIndex % period;

        const bool hasScrolled = (scrolledMask & (1u << cascadeIndex)) != 0;
        const bool isFirstTrace = m_shaderData.cascades[cascadeIndex].gridOrigin.z == 0;

        const GlimmerProbeDispatchMode mode = isFirstTrace ? GPDM_ALL : (hasScrolled ? GPDM_SCROLLED : GPDM_SLICE);
        const uint32 numProbes = mode == GPDM_SLICE ? GlimmerProbesPerCascade / period : GlimmerProbesPerCascade;

        cascadeDispatches[cascadeIndex] = Vec4u(uint32(mode), period | (slice << 16), numProbes, 0);
        maxProbes = MathUtil::Max(maxProbes, numProbes);
    }

    { // trace
        GlimmerProbeTraceConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;
        constants.spans = inputs.spanCache->GetShaderData();
        constants.dispatch = Vec4u(0, skyTextureIndex, 0, 0);
        Memory::Copy(constants.cascadeDispatches, cascadeDispatches, sizeof(cascadeDispatches));
        constants.sky = Vec4f(skyDiffuseStrength, MathUtil::Max(g_cvGlimmerSWRTFoliageExtinction.Get() * MathUtil::Clamp(g_cvGlimmerSWRTFoliageClumping.Get(), 0.0f, 1.0f), 0.0f), 0.0f, 0.0f);

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        for (const Handle<Texture>& shTexture : m_shTextures)
        {
            cr << InsertBarrier(shTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        }

        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_rayHitsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const StaticShaderPropertyId* modeProperty : { &s_propTraceModeTrace, &s_propTraceModeShade })
        {
            ShaderPropertySet shaderProperties;
            shaderProperties.Add(*modeProperty);

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeTrace"), shaderProperties));

            uint32 uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
            cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
            cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
            cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, tlasNodes.Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerInstancesBuffer"_sh, tlasInstances.Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceShaderData)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, inputs.blasCache->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, inputs.blasCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
            cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH0Texture"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[0]));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH1Texture"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[1]));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH2Texture"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[2]));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, RI.textureViewCache->GetOrCreate(m_baseTexture));
            cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
            cr << SetShaderUniform(uniformIndex++, "OutRays"_sh, m_raysBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "RayHits"_sh, m_rayHitsBuffer.Get(), ShaderDataOffset(0, RayHitSize));

            cr << DispatchCompute(Vec3u { maxProbes / ProbesPerTraceGroup, GlimmerProbeCascades, 1 });

            // the shade pass reads the trace pass's hits and adds to its rays
            cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            cr << InsertBarrier(m_rayHitsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }
    }


    { // blend
        GlimmerProbeBlendConstants constants {};
        constants.volume = m_shaderData;
        Memory::Copy(constants.cascadeDispatches, cascadeDispatches, sizeof(cascadeDispatches));

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        for (const Handle<Texture>& shTexture : m_shTextures)
        {
            cr << InsertBarrier(shTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_trendTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeBlend")));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "Rays"_sh, m_raysBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, RI.textureViewCache->GetOrCreate(m_baseTexture));
        cr << SetShaderUniform(uniformIndex++, "OutProbeSH0"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[0]));
        cr << SetShaderUniform(uniformIndex++, "OutProbeSH1"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[1]));
        cr << SetShaderUniform(uniformIndex++, "OutProbeSH2"_sh, RI.textureViewCache->GetOrCreate(m_shTextures[2]));
        cr << SetShaderUniform(uniformIndex++, "OutProbeState"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
        cr << SetShaderUniform(uniformIndex++, "OutProbeTrend"_sh, RI.textureViewCache->GetOrCreate(m_trendTexture));

        cr << DispatchCompute(Vec3u { (maxProbes + BlendGroupSize - 1) / BlendGroupSize, GlimmerProbeCascades, 1 });
    }


    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerProbeCascades; cascadeIndex++)
    {
        m_shaderData.cascades[cascadeIndex].gridOrigin.z = 1;
    }

    for (const Handle<Texture>& shTexture : m_shTextures)
    {
        cr << InsertBarrier(shTexture->GetGpuImage(), ResourceState::ShaderResource);
    }

    cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_shaderData.info.w = 1;
}

} // namespace Hyperion
