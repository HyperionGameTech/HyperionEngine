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
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Material.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGrass.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

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

GlimmerTexelRect GetGlimmerGroundWindow(const Vec2i& origin)
{
    return GlimmerTexelRect { origin, origin + Vec2i(int32(GlimmerGroundResolution), int32(GlimmerGroundResolution)) };
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

float GetGlimmerNearFieldRadius()
{
    return MathUtil::Max(g_cvGlimmerSWRTNearFieldRadius.Get(), 8.0f);
}

float GetGlimmerSWRTRadius(const BoundingBox& sceneRegion)
{
    return MathUtil::Min(GetGlimmerNearFieldRadius(), 0.5f * sceneRegion.GetExtent().x);
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

Vec4f MakeGlimmerRandomRotation(uint32 seed)
{
    const float u1 = float(MathUtil::HashUint32(seed * 3 + 0) & 0xFFFFFFu) / float(0x1000000);
    const float u2 = float(MathUtil::HashUint32(seed * 3 + 1) & 0xFFFFFFu) / float(0x1000000);
    const float u3 = float(MathUtil::HashUint32(seed * 3 + 2) & 0xFFFFFFu) / float(0x1000000);

    const float a = MathUtil::Sqrt(1.0f - u1);
    const float b = MathUtil::Sqrt(u1);
    const float twoPi = 2.0f * MathUtil::pi<float>;

    return Vec4f(a * MathUtil::Sin(twoPi * u2), a * MathUtil::Cos(twoPi * u2), b * MathUtil::Sin(twoPi * u3), b * MathUtil::Cos(twoPi * u3));
}

GpuBufferRef CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, elementSize * MathUtil::Max(numElements, size_t(1)), alignof(Vec4f));
    Check(buffer->Create());

    return buffer;
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

void CollectGlimmerTerrainPatches(RenderProxyList& rpl, Array<GlimmerTerrainPatchShaderData>& outPatches)
{
    for (Entity* entity : rpl.GetMeshEntities())
    {
        RenderProxyMesh* proxy = rpl.GetMeshEntities().GetProxy(entity->Id());

        if (!proxy || !proxy->mesh || !proxy->material || proxy->numInstances != 0)
        {
            continue;
        }

        if (proxy->attributes.GetMaterialAttributes().shaderName != "Terrain"_sh)
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

        GlimmerTerrainPatchShaderData* existing = outPatches.FindIf([&patch](const GlimmerTerrainPatchShaderData& other)
            {
                return other.data.x == patch.data.x && other.worldToObject0 == patch.worldToObject0 && other.worldToObject2 == patch.worldToObject2;
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

float GetGlimmerFoliageExtinction()
{
    return MathUtil::Max(g_cvGlimmerSWRTFoliageExtinction.Get() * MathUtil::Clamp(g_cvGlimmerSWRTFoliageClumping.Get(), 0.0f, 1.0f), 0.0f);
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

} // namespace Hyperion
