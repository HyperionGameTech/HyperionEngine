/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/Shared.hpp>

#include <Rendering/Glimmer/GlimmerMemory.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class RenderProxyList;
class TerrainWorldGridLayer;

class EnvProbe;
class Material;

struct GlimmerChannelState;
struct GlimmerTerrainPatchShaderData;
struct EnvProbeShaderData;

// luminance the sky (and emissive surfaces) seen by Glimmer's rays is clamped to
static constexpr float GlimmerSkyMaxLuminance = 64.0f;

// how far the probe and SH voxel rays look before taking the sky
static constexpr float GlimmerMaxRayDistance = 2000.0f;

static constexpr float GlimmerDefaultGroundAlbedo = 0.25f;

static constexpr uint32 GlimmerMaxLightmapPages = 4;

static constexpr uint32 GlimmerLightTerms = 9;

static constexpr uint32 GlimmerLightParts = GlimmerLightTerms + 1;

// NOTE must match the layout in GlimmerLight.hlsli
static constexpr uint32 GlimmerLightFullTerms = 4;
static constexpr uint32 GlimmerLightScalarTerms = GlimmerLightTerms - GlimmerLightFullTerms;
static constexpr uint32 GlimmerLightTransferEntries = GlimmerLightFullTerms * 3 + (GlimmerLightScalarTerms * 3 + 3) / 4 + GlimmerLightTerms;
static constexpr uint32 GlimmerLightEntries = GlimmerLightTransferEntries + 3;

struct GlimmerLightShaderData
{
    Vec4f bounceWeights[GlimmerLightTerms]; // rgb = the sun's and the sky's SH term
    Vec4f directWeights[GlimmerLightTerms]; // rgb = the sky's alone

    GlimmerLightShaderData()
    {
        Memory::Zero(this, sizeof(GlimmerLightShaderData));
    }
};

struct GlimmerSkyShaderData
{
    Vec4u info;   // x = sky probe color texture index (~0 without one)
    Vec4f params; // x = sky probe diffuse strength, y = luminance escaping rays are clamped to, z = luminance of the sky's irradiance from above (without the world's sky intensity)
};

struct GlimmerTexelRect
{
    Vec2i min;
    Vec2i max;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return max.x <= min.x || max.y <= min.y;
    }

    HYP_FORCE_INLINE int32 Area() const
    {
        return IsEmpty() ? 0 : (max.x - min.x) * (max.y - min.y);
    }

    static GlimmerTexelRect Intersect(const GlimmerTexelRect& a, const GlimmerTexelRect& b);
    static GlimmerTexelRect Union(const GlimmerTexelRect& a, const GlimmerTexelRect& b);
};

struct GlimmerLightmapPages
{
    Texture* irradianceTextures[GlimmerMaxLightmapPages] = {};
    Vec4u stencilValues;
};

struct GlimmerBox
{
    Vec3i min;
    Vec3i max; // exclusive

    HYP_FORCE_INLINE int64 Volume() const
    {
        return int64(max.x - min.x) * int64(max.y - min.y) * int64(max.z - min.z);
    }

    static GlimmerBox Union(const GlimmerBox& a, const GlimmerBox& b)
    {
        return GlimmerBox {
            MathUtil::Min(a.min, b.min),
            MathUtil::Max(a.max, b.max)
        };
    }
};


namespace GlimmerHelpers {

GlimmerTexelRect GetGlimmerGroundWindow(const Vec2i& origin);

void GetGlimmerScrolledRects(const GlimmerTexelRect& oldWindow, const GlimmerTexelRect& newWindow, GlimmerTexelRect& outColumns, GlimmerTexelRect& outRows);

bool IsGlimmerSceneRequired();

float GetGlimmerRegionMinRadius();

float GetGlimmerSHCascadeSpacing(uint32 cascadeIndex);
float GetGlimmerSHOccupancySpacing(uint32 cascadeIndex);

float GetGlimmerProbeLevelSpacing(uint32 levelIndex);

Vec3f GetGlimmerWindowCenter(const Vec3f& viewerPosition, const Vec3f& lookAhead, const Vec3f& windowExtent);
Vec3f GetGlimmerProbeWindowCenter(const Vec3f& viewerPosition, const Vec3f& lookAhead, uint32 levelIndex);
Vec3i GetGlimmerProbeWindowOrigin(const Vec3f& viewerPosition, const Vec3f& lookAhead, uint32 levelIndex);

Vec4f MakeGlimmerRandomRotation(uint32 seed);

// GPU only: written by shaders, or by staging copies recorded with the frame. elementSize is the stride the shaders declare.
StructuredBuffer CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements);
RWStructuredBuffer CreateGlimmerRWBuffer(size_t elementSize, size_t numElements);

void ClearGlimmerBuffer(const RawBuffer& buffer);

// grows \p buffer to at least \p byteSize, without keeping what it held
void EnsureGlimmerBufferCapacity(RawBuffer& buffer, size_t byteSize);

Handle<Texture> CreateGlimmerStorageTexture(TextureType type, TextureFormat format, const Vec3u& extent, uint16 numLayers, Name name, TextureWrapMode wrapMode = TextureWrapMode::Repeat);

uint32 CalculateGlimmerMipChainCells(uint32 resolution, uint32 numLevels);

void CollectGlimmerTerrainPatches(RenderProxyList& rpl, Array<GlimmerTerrainPatchShaderData, GlimmerTempAllocator>& outPatches);

void FillGlimmerGroundCover(TerrainWorldGridLayer* terrain, GlimmerChannelState& outState);

// the sky as both the probe and SH voxel traces see it
void GetGlimmerSkyShaderData(EnvProbe* skyProbe, GlimmerSkyShaderData& outSky, EnvProbeShaderData& outSkyProbe);

void GetGlimmerLightShaderData(EnvProbe* skyProbe, GlimmerLightShaderData& outLight);

float GetGlimmerFoliageExtinction();

void CollectGlimmerLightmapPages(RenderProxyList& rpl, GlimmerLightmapPages& outPages);

uint64 GetMaterialHash(RenderProxyList& rpl, const Material& material);

} // namespace GlimmerHelpers

} // namespace Hyperion
