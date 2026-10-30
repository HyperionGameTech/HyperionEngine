/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GlimmerSceneInstances;
class GlimmerTriangleCache;
class GlimmerSurfaceCache;

static constexpr uint32 GlimmerSpanValuesPerTexel = 22;
static constexpr uint32 GlimmerSpanMaxRects = 4;

struct GlimmerSpanLevelShaderData
{
    Vec4i window; // xy = absolute texel the spans were built from, z = 1 when built
    Vec4f params; // x = texel size, y = 1 / texel size
};

struct GlimmerSpanShaderData
{
    GlimmerSpanLevelShaderData levels[GlimmerGroundLevels];
};

static constexpr float GlimmerSpansMinFoliageHeight = 1.5f;

static constexpr uint32 GlimmerHeightBoundsTileTexels = 16;
static constexpr uint32 GlimmerHeightBoundsTiles = GlimmerGroundResolution / GlimmerHeightBoundsTileTexels;
static constexpr uint32 GlimmerHeightBoundsStride = 2 * GlimmerHeightBoundsTiles * GlimmerHeightBoundsTiles + 4;

class GlimmerSpanCache final
{
public:
    GlimmerSpanCache();

    GlimmerSpanCache(const GlimmerSpanCache& other) = delete;
    GlimmerSpanCache& operator=(const GlimmerSpanCache& other) = delete;

    ~GlimmerSpanCache();

    void Update(Frame* frame, const GlimmerChannelState& state, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache, const GlimmerSurfaceCache& surfaceCache);

    HYP_FORCE_INLINE const GlimmerSpanShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetSpansBuffer() const
    {
        return m_spansBuffer;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetHeightBoundsBuffer() const
    {
        return m_heightBoundsBuffer;
    }

    HYP_FORCE_INLINE const GlimmerTexelRect& GetFilledRect(uint32 level) const
    {
        return m_lastFilledRects[level];
    }

private:
    using Rect = GlimmerTexelRect;

    struct RectList
    {
        Rect rects[GlimmerSpanMaxRects];
        uint32 count = 0;

        // false when full
        bool Add(const Rect& rect);
    };

    void FillLevel(
        Frame* frame,
        uint32 levelIndex,
        const Vec2i& windowOrigin,
        const RectList& rects,
        const GlimmerSceneInstances& sceneInstances,
        const GlimmerTriangleCache& triangleCache,
        const GlimmerSurfaceCache& surfaceCache);

    void UpdateHeightBounds(Frame* frame, const GlimmerChannelState& state, const GlimmerSurfaceCache& surfaceCache);

    RWStructuredBuffer m_spansBuffer;

    RWStructuredBuffer m_heightBoundsBuffer;
    Rect m_filledRects[GlimmerGroundLevels];
    Rect m_lastFilledRects[GlimmerGroundLevels];
    Vec2i m_boundsWindowOrigins[GlimmerGroundLevels];
    Vec4i m_boundsValidRects[GlimmerGroundLevels];
    Vec4i m_boundsSpanWindows[GlimmerGroundLevels];

    uint32 m_builtGenerations[GlimmerGroundLevels];
    Vec2i m_builtOrigins[GlimmerGroundLevels];

    // what scene instance swaps changed since the level was last filled, waiting for its turn
    RectList m_changed[GlimmerGroundLevels];
    bool m_changedWholeWindow[GlimmerGroundLevels];
    uint32 m_seenGeneration;

    // filled while the ground under them hadn't loaded, so foliage there wasn't told apart from ground cover
    RectList m_filledWithoutGround[GlimmerGroundLevels];

    GlimmerSpanShaderData m_shaderData;
};

} // namespace Hyperion
