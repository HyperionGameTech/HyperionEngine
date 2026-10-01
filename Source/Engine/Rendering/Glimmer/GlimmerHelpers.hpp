/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class RenderProxyList;
class TerrainWorldGridLayer;

struct BoundingBox;
struct GlimmerChannelState;
struct GlimmerTerrainPatchShaderData;

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

} // namespace Hyperion
