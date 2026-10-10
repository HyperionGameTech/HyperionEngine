/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

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
struct BoundingBox;
struct GlimmerChannelState;
struct GlimmerTerrainPatchShaderData;
struct EnvProbeShaderData;

// luminance the sky (and emissive surfaces) seen by Glimmer's rays is clamped to
static constexpr float GlimmerSkyMaxLuminance = 64.0f;

// how far the probe and SH voxel rays look before taking the sky
static constexpr float GlimmerMaxRayDistance = 2000.0f;

static constexpr uint32 GlimmerMaxLightmapPages = 4;

enum class GlimmerLightingChangeFlags : uint32
{
    None = 0x0,
    Sun = 0x1, //!< its direction, colour or intensity
    Sky = 0x2  //!< its brightness
};

HYP_MAKE_ENUM_FLAGS(GlimmerLightingChangeFlags);

struct GlimmerSkyShaderData
{
    Vec4u info;   // x = sky probe color texture index (~0 without one)
    Vec4f params; // x = sky probe diffuse strength, y = luminance escaping rays are clamped to, z = luminance of the sky's irradiance from above (without the world's sky intensity)
};

/// @TODO: Namespace this

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

GlimmerTexelRect GetGlimmerGroundWindow(const Vec2i& origin);

void GetGlimmerScrolledRects(const GlimmerTexelRect& oldWindow, const GlimmerTexelRect& newWindow, GlimmerTexelRect& outColumns, GlimmerTexelRect& outRows);

float GetGlimmerNearFieldRadius();

// SWRT only covers the middle of the scene region; the rest is only splatted into the heightfield
float GetGlimmerSWRTRadius(const BoundingBox& sceneRegion);

float GetGlimmerSHCascadeSpacing(uint32 cascadeIndex);
float GetGlimmerSHOccupancySpacing(uint32 cascadeIndex);

float GetGlimmerProbeLevelSpacing(uint32 levelIndex);

Vec4f MakeGlimmerRandomRotation(uint32 seed);

/// @TODO: Return a `StructuredBuffer`!
GpuBufferRef CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements);

Handle<Texture> CreateGlimmerStorageTexture(TextureType type, TextureFormat format, const Vec3u& extent, uint16 numLayers, Name name, TextureWrapMode wrapMode = TextureWrapMode::Repeat);

uint32 CalculateGlimmerMipChainCells(uint32 resolution, uint32 numLevels);

void CollectGlimmerTerrainPatches(RenderProxyList& rpl, Array<GlimmerTerrainPatchShaderData>& outPatches);

void FillGlimmerGroundCover(TerrainWorldGridLayer* terrain, GlimmerChannelState& outState);

// the sky as both the probe and SH voxel traces see it
void GetGlimmerSkyShaderData(EnvProbe* skyProbe, GlimmerSkyShaderData& outSky, EnvProbeShaderData& outSkyProbe);

float GetGlimmerFoliageExtinction();

struct GlimmerLightmapPages
{
    Texture* irradianceTextures[GlimmerMaxLightmapPages] = {};
    Vec4u stencilValues;
};

void CollectGlimmerLightmapPages(RenderProxyList& rpl, GlimmerLightmapPages& outPages);

} // namespace Hyperion
