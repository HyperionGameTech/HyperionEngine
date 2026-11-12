/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/RenderHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGrass.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

extern CVar<bool> g_cvLightmapVolumes;

#pragma region GlimmerTexelRect

GlimmerTexelRect GlimmerTexelRect::Intersect(const GlimmerTexelRect& a, const GlimmerTexelRect& b)
{
    GlimmerTexelRect result;
    result.min = Vec2i(MathUtil::Max(a.min.x, b.min.x), MathUtil::Max(a.min.y, b.min.y));
    result.max = Vec2i(MathUtil::Min(a.max.x, b.max.x), MathUtil::Min(a.max.y, b.max.y));

    if (result.IsEmpty())
    {
        result.max = result.min;
    }

    return result;
}

GlimmerTexelRect GlimmerTexelRect::Union(const GlimmerTexelRect& a, const GlimmerTexelRect& b)
{
    if (a.IsEmpty())
    {
        return b;
    }

    if (b.IsEmpty())
    {
        return a;
    }

    GlimmerTexelRect result;
    result.min = Vec2i(MathUtil::Min(a.min.x, b.min.x), MathUtil::Min(a.min.y, b.min.y));
    result.max = Vec2i(MathUtil::Max(a.max.x, b.max.x), MathUtil::Max(a.max.y, b.max.y));

    return result;
}

#pragma endregion GlimmerTexelRect

namespace GlimmerHelpers {

GlimmerTexelRect GetGlimmerGroundWindow(const Vec2i& origin)
{
    return GlimmerTexelRect {
        origin,
        origin + Vec2i(int32(GlimmerGroundResolution), int32(GlimmerGroundResolution))
    };
}

void GetGlimmerScrolledRects(const GlimmerTexelRect& oldWindow, const GlimmerTexelRect& newWindow, GlimmerTexelRect& outColumns, GlimmerTexelRect& outRows)
{
    outColumns = newWindow;
    outRows = newWindow;

    if (newWindow.min.x > oldWindow.min.x)
    {
        outColumns.min.x = MathUtil::Max(oldWindow.max.x, newWindow.min.x);
    }
    else
    {
        outColumns.max.x = MathUtil::Min(oldWindow.min.x, newWindow.max.x);
    }

    outRows.min.x = MathUtil::Max(newWindow.min.x, oldWindow.min.x);
    outRows.max.x = MathUtil::Min(newWindow.max.x, oldWindow.max.x);

    if (newWindow.min.y > oldWindow.min.y)
    {
        outRows.min.y = MathUtil::Max(oldWindow.max.y, newWindow.min.y);
    }
    else
    {
        outRows.max.y = MathUtil::Min(oldWindow.min.y, newWindow.max.y);
    }
}

bool IsGlimmerSceneRequired()
{
    return g_cvGlimmerEnabled.Get() || g_cvGlimmerDebugView.Get() >= int(GlimmerDebugView::TechniqueFirst);
}

float GetGlimmerRegionMinRadius()
{
    return 96.0f;
}

float GetGlimmerSHCascadeSpacing(uint32 cascadeIndex)
{
    constexpr float CascadeScales[GlimmerSHCascades] = { 0.5f, 1.0f, 2.0f, 8.0f, 32.0f };

    return GlimmerSHSpacing * CascadeScales[MathUtil::Min(cascadeIndex, GlimmerSHCascades - 1)];
}

float GetGlimmerSHOccupancySpacing(uint32 cascadeIndex)
{
    return 0.5f * GetGlimmerSHCascadeSpacing(cascadeIndex);
}

Vec3f GetGlimmerWindowCenter(const Vec3f& viewerPosition, const Vec3f& lookAhead, const Vec3f& windowExtent)
{
    const Vec2f horizontal = Vec2f(lookAhead.x, lookAhead.z);
    const float maxHorizontal = 0.25f * MathUtil::Min(windowExtent.x, windowExtent.z);

    const float horizontalLength = horizontal.Length();

    Vec2f clampedHorizontal = horizontal;

    if (horizontalLength > maxHorizontal)
    {
        clampedHorizontal = horizontal * (maxHorizontal / horizontalLength);
    }

    const float clampedVertical = MathUtil::Clamp(lookAhead.y, -0.25f * windowExtent.y, 0.25f * windowExtent.y);

    return viewerPosition + Vec3f(clampedHorizontal.x, clampedVertical, clampedHorizontal.y);
}

Vec4f MakeGlimmerRandomRotation(uint32 seed)
{
    constexpr float TwoPi = 2.0f * MathUtil::pi<float>;

    const float u1 = float(MathUtil::HashUint32(seed * 3 + 0) & 0xFFFFFFu) / float(0x1000000);
    const float u2 = float(MathUtil::HashUint32(seed * 3 + 1) & 0xFFFFFFu) / float(0x1000000);
    const float u3 = float(MathUtil::HashUint32(seed * 3 + 2) & 0xFFFFFFu) / float(0x1000000);

    const float a = MathUtil::Sqrt(1.0f - u1);
    const float b = MathUtil::Sqrt(u1);

    return {
        a * MathUtil::Sin(TwoPi * u2),
        a * MathUtil::Cos(TwoPi * u2),
        b * MathUtil::Sin(TwoPi * u3),
        b * MathUtil::Cos(TwoPi * u3)
    };
}

GpuBufferRef CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(
        GpuBufferType::StructuredBuffer,
        elementSize * MathUtil::Max(numElements, size_t(1)),
        alignof(Vec4f));

    Check(buffer->Create());

    return buffer;
}

GpuBufferRef CreateGlimmerRWBuffer(size_t byteSize, size_t alignment)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, byteSize, alignment);
    Check(buffer->Create());

    ClearGlimmerBuffer(buffer.Get());

    return buffer;
}

void ClearGlimmerBuffer(GpuBuffer* buffer)
{
    const size_t size = buffer->Size();

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(size);
    Assert(stagingBuffer != nullptr);

    stagingBuffer->Memset(size, 0);
    stagingBuffer->Flush(0, size);

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder(CommandRecorderQueue::PreRender);

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(buffer, ResourceState::CopyDst);
    cr << CopyBuffer(stagingBuffer, buffer, uint32(size));
    cr << InsertBarrier(buffer, ResourceState::UnorderedAccess);

    cr.Done();
}

Handle<Texture> CreateGlimmerStorageTexture(TextureType type, TextureFormat format, const Vec3u& extent, uint16 numLayers, Name name, TextureWrapMode wrapMode)
{
    Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
        type,
        format,
        extent,
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        wrapMode,
        numLayers,
        ImageUsage::Storage | ImageUsage::Sampled });

    texture->SetIsTransient(true);
    texture->SetName(name);
    Check(texture->Create());

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder(CommandRecorderQueue::PreRender);

    cr << InsertBarrier(texture->GetGpuImage(), ResourceState::CopyDst);
    cr << FillImage(texture->GetGpuImage(), 0.0f, ImageSubResource { .numLayers = numLayers });
    cr << InsertBarrier(texture->GetGpuImage(), ResourceState::ShaderResource);

    cr.Done();

    return texture;
}

uint32 CalculateGlimmerMipChainCells(uint32 resolution, uint32 numLevels)
{
    uint32 totalCells = 0;

    for (uint32 level = 0; level < numLevels; level++)
    {
        const uint32 levelResolution = MathUtil::Max(resolution >> level, 1u);
        totalCells += levelResolution * levelResolution;
    }

    return totalCells;
}

void CollectGlimmerTerrainPatches(
    RenderProxyList& rpl,
    Array<GlimmerTerrainPatchShaderData, GlimmerTempAllocator>& outPatches)
{
    for (Entity* entity : rpl.GetMeshEntities())
    {
        RenderProxyMesh* proxy = rpl.GetMeshEntities().GetProxy(entity->Id());

        if (!proxy || !proxy->mesh || !proxy->material || proxy->numInstances != 0)
        {
            continue;
        }

        if (!(RenderHelpers::GetShaderTraits(proxy->attributes.GetMaterialAttributes().shaderName) & ShaderTraits::Terrain))
        {
            continue;
        }

        const uint32 materialIndex = Resources::GetBinding(proxy->material);

        if (materialIndex == ~0u)
        {
            continue;
        }

        const Mat4f& objectToWorld = proxy->bufferData.modelMatrix;
        const Mat4f worldToObject = objectToWorld.Inverse();
        const BoundingBox worldBounds = objectToWorld * proxy->meshAabb;

        if (!worldBounds.IsValid() || !worldBounds.IsFinite())
        {
            continue;
        }

        GlimmerTerrainPatchShaderData patch {};
        Memory::Copy(&patch.worldToObject0, &worldToObject.values[0], sizeof(Vec4f));
        Memory::Copy(&patch.worldToObject2, &worldToObject.values[8], sizeof(Vec4f));
        patch.boundsXZ = Vec4f(worldBounds.min.x, worldBounds.min.z, worldBounds.max.x, worldBounds.max.z);
        patch.data = Vec4u(materialIndex, 0, 0, 0);

        GlimmerTerrainPatchShaderData* existing = outPatches.FindIf(
            [&patch](const GlimmerTerrainPatchShaderData& other)
            {
                return other.data.x == patch.data.x
                    && other.worldToObject0 == patch.worldToObject0
                    && other.worldToObject2 == patch.worldToObject2;
            });

        if (existing != outPatches.End())
        {
            existing->boundsXZ = Vec4f(
                MathUtil::Min(existing->boundsXZ.x, patch.boundsXZ.x),
                MathUtil::Min(existing->boundsXZ.y, patch.boundsXZ.y),
                MathUtil::Max(existing->boundsXZ.z, patch.boundsXZ.z),
                MathUtil::Max(existing->boundsXZ.w, patch.boundsXZ.w));

            continue;
        }

        if (outPatches.Size() < GlimmerMaxTerrainPatches)
        {
            outPatches.PushBack(patch);
        }
    }
}

void GetGlimmerSkyShaderData(EnvProbe* skyProbe, GlimmerSkyShaderData& outSky, EnvProbeShaderData& outSkyProbe)
{
    // default constructed without a sky probe, which has textureIndices ~0u
    outSkyProbe = EnvProbeShaderData {};
    outSky.info = Vec4u(~0u, 0, 0, 0);
    outSky.params = Vec4f(0.0f, GlimmerSkyMaxLuminance, 0.0f, 0.0f);

    if (!skyProbe)
    {
        return;
    }

    if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(skyProbe)))
    {
        outSkyProbe = skyProbeProxy->bufferData;
        outSky.info.x = skyProbeProxy->bufferData.textureIndices & 0xFFFFu;
        outSky.params.x = skyProbeProxy->bufferData.worldPosition.w;

        // the sky's irradiance on an upward surface (ProjectSHBands for (0, 1, 0)), before the world's sky intensity
        const Vec4f* sh = skyProbeProxy->bufferData.shData;
        const Vec4f up = sh[0] * 0.282095f + sh[1] * 0.488603f - sh[6] * 0.315392f - sh[8] * 0.546274f;

        outSky.params.z = (MathUtil::Max(up.x, 0.0f) * 0.2126f + MathUtil::Max(up.y, 0.0f) * 0.7152f + MathUtil::Max(up.z, 0.0f) * 0.0722f)
            * skyProbeProxy->bufferData.worldPosition.w;

        if (const WorldShaderData* worldData = GetWorldBufferData())
        {
            outSky.params.w = skyProbeProxy->bufferData.worldPosition.w * worldData->skyLightParams.x;
        }
    }
}

static constexpr float GlimmerSkyMidY = 0.642788f;
static constexpr float GlimmerSkyBelowY = 0.17f;
static constexpr uint32 GlimmerSkyMidBins = 3;
static constexpr uint32 GlimmerSkyHorizonBins = 4;

static constexpr float GlimmerSkyTermSolidAngle[GlimmerLightTerms] = { 1.122216f, 1.047198f, 1.047198f, 1.047198f, 0.638362f, 0.638362f, 0.638362f, 0.638362f, 5.749115f };

static Vec4f s_skyBins[GlimmerLightTerms] {};
static uint32 s_skyBinsTextureIndex = ~0u;

void GetGlimmerSkyTerms(const Vec3f& direction, float outTerms[GlimmerLightTerms])
{
    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        outTerms[term] = 0.0f;
    }

    const float below = MathUtil::Clamp(-direction.y / GlimmerSkyBelowY, 0.0f, 1.0f);
    const float y = MathUtil::Max(direction.y, 0.0f);

    const float horizon = MathUtil::Clamp(1.0f - y / GlimmerSkyMidY, 0.0f, 1.0f) * (1.0f - below);
    const float zenith = MathUtil::Clamp((y - GlimmerSkyMidY) / (1.0f - GlimmerSkyMidY), 0.0f, 1.0f) * (1.0f - below);
    const float mid = (1.0f - below) - horizon - zenith;

    float azimuth = std::atan2(direction.z, direction.x);

    if (azimuth < 0.0f)
    {
        azimuth += 6.28318531f;
    }

    const float midAt = azimuth * (float(GlimmerSkyMidBins) / 6.28318531f);
    const uint32 midBin = MathUtil::Min(uint32(midAt), GlimmerSkyMidBins - 1);
    const float midBlend = MathUtil::Clamp(midAt - float(midBin), 0.0f, 1.0f);

    const float horizonAt = azimuth * (float(GlimmerSkyHorizonBins) / 6.28318531f);
    const uint32 horizonBin = MathUtil::Min(uint32(horizonAt), GlimmerSkyHorizonBins - 1);
    const float horizonBlend = MathUtil::Clamp(horizonAt - float(horizonBin), 0.0f, 1.0f);

    outTerms[0] = zenith;
    outTerms[1 + midBin] += mid * (1.0f - midBlend);
    outTerms[1 + (midBin + 1) % GlimmerSkyMidBins] += mid * midBlend;
    outTerms[4 + horizonBin] += horizon * (1.0f - horizonBlend);
    outTerms[4 + (horizonBin + 1) % GlimmerSkyHorizonBins] += horizon * horizonBlend;
    outTerms[8] = below;
}

void SetGlimmerSkyBins(uint32 skyTextureIndex, const Vec4f (&radiance)[GlimmerLightTerms])
{
    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        s_skyBins[term] = radiance[term];
    }

    s_skyBinsTextureIndex = skyTextureIndex;
}

// until GlimmerSkyBins has measured the sky probe: its radiance SH, averaged over each term
static void GetSkyTermsFromSH(const Vec4f* irradianceSH, Vec4f (&outRadiance)[GlimmerLightTerms])
{
    constexpr float RadianceFromIrradiance[9] = { 1.0f, 1.5f, 1.5f, 1.5f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f };
    constexpr uint32 NumSamples = 512;

    float weightSums[GlimmerLightTerms] {};

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        outRadiance[term] = Vec4f(0.0f);
    }

    for (uint32 sampleIndex = 0; sampleIndex < NumSamples; sampleIndex++)
    {
        const float y = 1.0f - 2.0f * (float(sampleIndex) + 0.5f) / float(NumSamples);
        const float radius = MathUtil::Sqrt(MathUtil::Max(1.0f - y * y, 0.0f));
        const float angle = float(sampleIndex) * 2.39996323f;

        const Vec3f d(radius * std::cos(angle), y, radius * std::sin(angle));

        const float sh[9] = {
            0.282095f,
            0.488603f * d.y,
            0.488603f * d.z,
            0.488603f * d.x,
            1.092548f * d.x * d.y,
            1.092548f * d.y * d.z,
            0.315392f * (3.0f * d.z * d.z - 1.0f),
            1.092548f * d.x * d.z,
            0.546274f * (d.x * d.x - d.y * d.y)
        };

        Vec3f radiance(0.0f);

        for (uint32 term = 0; term < 9; term++)
        {
            radiance += irradianceSH[term].GetXYZ() * (sh[term] * RadianceFromIrradiance[term]);
        }

        radiance = Vec3f(MathUtil::Max(radiance.x, 0.0f), MathUtil::Max(radiance.y, 0.0f), MathUtil::Max(radiance.z, 0.0f));

        float terms[GlimmerLightTerms];
        GetGlimmerSkyTerms(d, terms);

        for (uint32 term = 0; term < GlimmerLightTerms; term++)
        {
            outRadiance[term] += Vec4f(radiance * terms[term], 0.0f);
            weightSums[term] += terms[term];
        }
    }

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        outRadiance[term] /= MathUtil::Max(weightSums[term], 1e-6f);
        outRadiance[term].w = weightSums[term] / float(NumSamples);
    }
}

// the inverse of the terms' Gram matrix (the integral of each pair's product over the sphere)
static constexpr float GlimmerSkyTermsGramInverse[GlimmerLightTerms][GlimmerLightTerms] = {
    { 1.489053f, -0.304822f, -0.304823f, -0.304823f, 0.120937f, 0.120938f, 0.120938f, 0.120938f, -0.003865f },
    { -0.304822f, 2.792408f, -0.481736f, -0.481736f, -1.140950f, -0.062060f, 0.297570f, -0.062060f, 0.007729f },
    { -0.304823f, -0.481736f, 2.777006f, -0.466334f, 0.207662f, -0.946308f, -0.511598f, 0.282742f, 0.007729f },
    { -0.304823f, -0.481736f, -0.466334f, 2.777006f, 0.207662f, 0.282742f, -0.511598f, -0.946308f, 0.007729f },
    { 0.120937f, -1.140950f, 0.207662f, 0.207662f, 4.580349f, -1.151015f, 0.463179f, -1.151015f, -0.021901f },
    { 0.120938f, -0.062060f, -0.946308f, 0.282742f, -1.151015f, 4.457706f, -1.032511f, 0.467316f, -0.021901f },
    { 0.120938f, 0.297570f, -0.511598f, -0.511598f, 0.463179f, -1.032511f, 4.343339f, -1.032510f, -0.021901f },
    { 0.120938f, -0.062060f, 0.282742f, -0.946308f, -1.151015f, 0.467316f, -1.032510f, 4.457706f, -0.021901f },
    { -0.003865f, 0.007729f, 0.007729f, 0.007729f, -0.021901f, -0.021901f, -0.021901f, -0.021901f, 0.180198f }
};

// the term weights whose blend matches the sky best (least squares), from each term's mean radiance (w = its share of the sphere):
// a term's mean alone blurs the sky across it, which a gap seeing only part of the term gets wrong
static void FitSkyTerms(const Vec4f (&meanRadiance)[GlimmerLightTerms], Vec3f (&outWeights)[GlimmerLightTerms])
{
    Vec3f projections[GlimmerLightTerms];

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        projections[term] = meanRadiance[term].GetXYZ() * (meanRadiance[term].w * 4.0f * 3.14159265f);
    }

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        Vec3f weight(0.0f);

        for (uint32 other = 0; other < GlimmerLightTerms; other++)
        {
            weight += projections[other] * GlimmerSkyTermsGramInverse[term][other];
        }

        outWeights[term] = Vec3f(MathUtil::Max(weight.x, 0.0f), MathUtil::Max(weight.y, 0.0f), MathUtil::Max(weight.z, 0.0f));
    }
}

void GetGlimmerLightShaderData(EnvProbe* skyProbe, GlimmerLightShaderData& outLight)
{
    outLight = GlimmerLightShaderData {};

    const WorldShaderData* worldData = GetWorldBufferData();

    if (!worldData)
    {
        return;
    }

    if (skyProbe)
    {
        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(skyProbe)))
        {
            const float skyScale = skyProbeProxy->bufferData.worldPosition.w * worldData->skyLightParams.x;

            Vec4f skyRadiance[GlimmerLightTerms];

            if (s_skyBinsTextureIndex == (skyProbeProxy->bufferData.textureIndices & 0xFFFFu))
            {
                for (uint32 term = 0; term < GlimmerLightTerms; term++)
                {
                    skyRadiance[term] = s_skyBins[term];
                }
            }
            else
            {
                GetSkyTermsFromSH(skyProbeProxy->bufferData.shData, skyRadiance);
            }

            Vec3f skyWeights[GlimmerLightTerms];
            FitSkyTerms(skyRadiance, skyWeights);

            for (uint32 term = 0; term < GlimmerLightTerms; term++)
            {
                outLight.directWeights[term] = Vec4f(skyWeights[term] * skyScale, 0.0f);
            }
        }
    }

    const Vec3f sunDirection = GetGlimmerSunDirection();

    const float aboveHorizon = MathUtil::Clamp((sunDirection.y + 0.1f) / 0.15f, 0.0f, 1.0f);

    const Vec3f sunIrradiance = worldData->sunColor.GetXYZ() * worldData->sunAtmosphereTint.GetXYZ() * (worldData->sunDirectionIntensity.w * aboveHorizon);

    float sunTerms[GlimmerLightTerms];
    GetGlimmerSkyTerms(sunDirection, sunTerms);

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        outLight.bounceWeights[term] = outLight.directWeights[term] + Vec4f(sunIrradiance * (sunTerms[term] / GlimmerSkyTermSolidAngle[term]), 0.0f);
    }

    outLight.sun = Vec4f(sunIrradiance, 0.0f);
}

Vec3f GetGlimmerSunDirection()
{
    const WorldShaderData* worldData = GetWorldBufferData();

    if (!worldData)
    {
        return Vec3f(0.0f, 1.0f, 0.0f);
    }

    const Vec3f toSun = worldData->sunDirectionIntensity.GetXYZ();

    if (toSun.LengthSquared() <= 1e-8f)
    {
        return Vec3f(0.0f, 1.0f, 0.0f);
    }

    return toSun.Normalized();
}

float GetGlimmerFoliageExtinction()
{
    // leaves bunch up in crowns with gaps between them, so a texel's leaf area blocks less than if it were spread evenly
    constexpr float FoliageClumping = 0.7f;

    return MathUtil::Max(g_cvGlimmerFoliageExtinction.Get() * FoliageClumping, 0.0f);
}

void CollectGlimmerLightmapPages(RenderProxyList& rpl, GlimmerLightmapPages& outPages)
{
    outPages = GlimmerLightmapPages {};

    if (!g_cvLightmapVolumes.Get())
    {
        return;
    }

    uint32 numPages = 0;

    for (LightmapVolume* lightmapVolume : rpl.GetLightmapVolumes())
    {
        const RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(lightmapVolume));

        if (!proxy || proxy->stencilBase == 0)
        {
            continue;
        }

        for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases && numPages < GlimmerMaxLightmapPages; atlasIndex++)
        {
            if (proxy->atlasIrradianceTextures[atlasIndex] == nullptr)
            {
                continue;
            }

            outPages.irradianceTextures[numPages] = proxy->atlasIrradianceTextures[atlasIndex];
            outPages.stencilValues[numPages] = uint32(proxy->stencilBase) + atlasIndex;

            numPages++;
        }
    }
}

void FillGlimmerGroundCover(TerrainWorldGridLayer* terrain, GlimmerChannelState& outState)
{
    HYP_SCOPE;

    const Array<TerrainCoverLayerPlan, TerrainAllocator>& plans = terrain->GetGroundCoverResources().GetPlans();
    const Array<TerrainCoverLayer, TerrainAllocator>& layers = terrain->GetGroundCoverResources().GetLayers();

    for (uint32 layerIndex = 0; layerIndex < uint32(MathUtil::Min(plans.Size(), layers.Size())); layerIndex++)
    {
        const TerrainCoverLayerPlan& plan = plans[layerIndex];

        // painted layers grow from paint that only the sim side has
        if (plan.isPainted || plan.splatLayer >= GlimmerGroundCoverLayers)
        {
            continue;
        }

        GlimmerGroundCoverLayerState& coverState = outState.groundCover[plan.splatLayer];

        // layers over the same splat layer hide what the others leave showing
        coverState.coverage = 1.0f - (1.0f - coverState.coverage) * (1.0f - MathUtil::Clamp(plan.coverage, 0.0f, 1.0f));

        for (uint32 typeIndex = 0; typeIndex < uint32(MathUtil::Min(plan.types.Size(), layers[layerIndex].types.Size())); typeIndex++)
        {
            const TerrainCoverType& type = layers[layerIndex].types[typeIndex];

            for (const TerrainCoverMember& member : type.members)
            {
                if (!member.material.IsValid() || coverState.numMaterials >= GlimmerGroundCoverMaxMaterials)
                {
                    continue;
                }

                coverState.materials[coverState.numMaterials] = member.material;
                coverState.weights[coverState.numMaterials] = plan.types[typeIndex].weight / float(MathUtil::Max(type.members.Size(), size_t(1)));
                coverState.numMaterials++;
            }
        }
    }
}

uint64 GetMaterialHash(RenderProxyList& rpl, const Material& material)
{
    const RenderProxyMaterial* materialProxy = rpl.GetMaterials().GetProxy(material.Id());

    if (!materialProxy)
    {
        return 0;
    }

    const MaterialShaderData& data = materialProxy->bufferData;

    uint64 hash = FNV1::DoHashWords(&data.albedo, sizeof(data.albedo), 0);
    hash = FNV1::DoHashWords(&data.packedParams, sizeof(data.packedParams), hash);
    hash = FNV1::DoHashWords(&data.textureUsage, sizeof(data.textureUsage), hash);
    hash = FNV1::DoHashWords(&data.uvScale, sizeof(data.uvScale), hash);

    for (const Texture* texture : materialProxy->boundTextures)
    {
        const uint64 textureId = texture ? uint64(texture->Id().Value()) : 0;

        hash = FNV1::DoHashWords(&textureId, sizeof(textureId), hash);
    }

    return hash;
}

} // namespace GlimmerHelpers

} // namespace Hyperion
