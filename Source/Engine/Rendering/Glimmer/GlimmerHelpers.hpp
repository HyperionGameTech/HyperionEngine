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

/*! \brief Absolute texels of a ground level, max exclusive. */
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

/*! \brief The texels a ground level's window covers while it sits at `origin`. */
GlimmerTexelRect GetGlimmerGroundWindow(const Vec2i& origin);

/*! \brief What scrolled into `newWindow` since `oldWindow`: a full height strip on x, then the rest of the new rows on z.
 *  The windows must overlap; either rect can come back empty. */
void GetGlimmerScrolledRects(const GlimmerTexelRect& oldWindow, const GlimmerTexelRect& newWindow, GlimmerTexelRect& outColumns, GlimmerTexelRect& outRows);

float GetGlimmerNearFieldRadius();

// SWRT only covers the middle of the scene region; the rest is only splatted into the heightfield
float GetGlimmerSWRTRadius(const BoundingBox& sceneRegion);

float GetGlimmerSHCascadeSpacing(uint32 cascadeIndex);
float GetGlimmerSHOccupancySpacing(uint32 cascadeIndex);

float GetGlimmerProbeLevelSpacing(uint32 levelIndex);

/*! \brief Uniformly random rotation quaternion (Shoemake), so each update's ray set samples new directions.
 *  Pairs with GlimmerRotateByQuaternion in Shaders/Glimmer/GlimmerCommon.hlsli. */
Vec4f MakeGlimmerRandomRotation(uint32 seed);

/*! \brief Created with room for at least one element, so an empty list still has something to bind. */
GpuBufferRef CreateGlimmerStructuredBuffer(size_t elementSize, size_t numElements);

/*! \brief Transient, unfiltered texture that compute passes write and later passes read. */
Handle<Texture> CreateGlimmerStorageTexture(TextureType type, TextureFormat format, const Vec3u& extent, uint16 numLayers, Name name, TextureWrapMode wrapMode = TextureWrapMode::Repeat);

/*! \brief Cells across every level of a square mip chain whose finest level is `resolution` wide. */
uint32 CalculateGlimmerMipChainCells(uint32 resolution, uint32 numLevels);

/*! \brief Terrain patches of a cell share its material and transform, so they merge into one entry covering all of them. */
void CollectGlimmerTerrainPatches(RenderProxyList& rpl, Array<GlimmerTerrainPatchShaderData>& outPatches);

void FillGlimmerGroundCover(TerrainWorldGridLayer* terrain, GlimmerChannelState& outState);

} // namespace Hyperion
