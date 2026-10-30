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
#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
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
    return GlimmerSHSpacing * float(1u << cascadeIndex);
}

float GetGlimmerSHOccupancySpacing(uint32 cascadeIndex)
{
    return 0.5f * GetGlimmerSHCascadeSpacing(cascadeIndex);
}

float GetGlimmerProbeLevelSpacing(uint32 levelIndex)
{
    return 2.0f * float(1u << levelIndex);
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

Vec3f GetGlimmerProbeWindowCenter(const Vec3f& viewerPosition, const Vec3f& lookAhead, uint32 levelIndex)
{
    const float blockSize = GetGlimmerProbeLevelSpacing(levelIndex) * float(GlimmerProbeBlock);

    return GetGlimmerWindowCenter(viewerPosition, lookAhead, Vec3f(blockSize * float(GlimmerProbeWindow)));
}

Vec3i GetGlimmerProbeWindowOrigin(const Vec3f& viewerPosition, const Vec3f& lookAhead, uint32 levelIndex)
{
    const int32 halfWindow = int32(GlimmerProbeWindow / 2);

    const float blockSize = GetGlimmerProbeLevelSpacing(levelIndex) * float(GlimmerProbeBlock);
    const Vec3f center = GetGlimmerProbeWindowCenter(viewerPosition, lookAhead, levelIndex);

    return Vec3i(MathUtil::Floor(center / blockSize + Vec3f(0.5f))) - halfWindow;
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

StructuredBuffer CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements)
{
    StructuredBuffer buffer(MathUtil::Max(numElements, size_t(1)), elementSize, RawBufferFlags::GpuOnly);
    buffer.Initialize();

    return buffer;
}

RWStructuredBuffer CreateGlimmerRWBuffer(size_t elementSize, size_t numElements)
{
    RWStructuredBuffer buffer(MathUtil::Max(numElements, size_t(1)), elementSize, RawBufferFlags::GpuOnly);
    buffer.Initialize();

    ClearGlimmerBuffer(buffer);

    return buffer;
}

void ClearGlimmerBuffer(const RawBuffer& buffer)
{
    const size_t size = buffer.TotalSizeBytes();

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(size);
    Assert(stagingBuffer != nullptr);

    stagingBuffer->Memset(size, 0);
    stagingBuffer->Flush(0, size);

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder(CommandRecorderQueue::PreRender);

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(buffer, ResourceState::CopyDst);
    cr << CopyBuffer(stagingBuffer, buffer.gpuBuffer, uint32(size));
    cr << InsertBarrier(buffer, ResourceState::UnorderedAccess);

    cr.Done();
}

void EnsureGlimmerBufferCapacity(RawBuffer& buffer, size_t byteSize)
{
    if (byteSize > buffer.TotalSizeBytes())
    {
        buffer.Resize(byteSize);
    }
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

        if (!RenderHelpers::IsTerrainShader(proxy->attributes.GetMaterialAttributes().shaderName))
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
            constexpr float RadianceFromIrradiance[9] = { 1.0f, 1.5f, 1.5f, 1.5f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f };

            const Vec4f* sh = skyProbeProxy->bufferData.shData;
            const float skyScale = skyProbeProxy->bufferData.worldPosition.w * worldData->skyLightParams.x;

            for (uint32 term = 0; term < GlimmerLightTerms; term++)
            {
                outLight.directWeights[term] = Vec4f(sh[term].GetXYZ() * (RadianceFromIrradiance[term] * skyScale), 0.0f);
            }
        }
    }

    const Vec3f toSun = worldData->sunDirectionIntensity.GetXYZ();
    const Vec3f sunDirection = toSun.LengthSquared() > 1e-8f ? toSun.Normalized() : Vec3f(0.0f, 1.0f, 0.0f);

    const float aboveHorizon = MathUtil::Clamp((sunDirection.y + 0.1f) / 0.15f, 0.0f, 1.0f);

    const Vec3f sunIrradiance = worldData->sunColor.GetXYZ() * worldData->sunAtmosphereTint.GetXYZ() * (worldData->sunDirectionIntensity.w * aboveHorizon);

    const float sunBasis[9] = {
        0.282095f,
        0.488603f * sunDirection.y,
        0.488603f * sunDirection.z,
        0.488603f * sunDirection.x,
        1.092548f * sunDirection.x * sunDirection.y,
        1.092548f * sunDirection.y * sunDirection.z,
        0.315392f * (3.0f * sunDirection.z * sunDirection.z - 1.0f),
        1.092548f * sunDirection.x * sunDirection.z,
        0.546274f * (sunDirection.x * sunDirection.x - sunDirection.y * sunDirection.y)
    };

    constexpr float WindowOfTerm[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        outLight.bounceWeights[term] = outLight.directWeights[term] + Vec4f(sunIrradiance * (sunBasis[term] * WindowOfTerm[term]), 0.0f);
    }
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

    const Array<TerrainCoverLayerPlan>& plans = terrain->GetGroundCoverResources().GetPlans();
    const Array<TerrainCoverLayer>& layers = terrain->GetGroundCoverResources().GetLayers();

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
