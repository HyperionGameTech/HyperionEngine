/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GlimmerTLAS;
class GlimmerBLASCache;
class GlimmerSurfaceCache;

static constexpr uint32 GlimmerSpanValuesPerTexel = 12;

// most rects of a level one splat fills; more than this and it fills the whole window instead
static constexpr uint32 GlimmerSpanMaxRects = 4;

// Must match GlimmerSpanLevel in Shaders/Glimmer/GlimmerCommon.hlsli
struct GlimmerSpanLevelShaderData
{
    Vec4i window; // xy = absolute texel the spans were built from, z = 1 when built
    Vec4f params; // x = texel size, y = 1 / texel size
};

struct GlimmerSpanShaderData
{
    GlimmerSpanLevelShaderData levels[GlimmerGroundLevels];
};

// foliage lower than this hugs the ground (grass, ferns), far thinner than the probes above it can resolve
static constexpr float GlimmerSpansMinFoliageHeight = 1.5f;

/*! \brief The heightfield's occupancy above the ground: per texel, the vertical span of static solids and of the foliage canopy,
 *  with the canopy's leaf area and the albedos of both. Built on the GPU by splatting the span instances' BLAS triangles,
 *  one level per frame whenever the instances or that level's window change. Render thread only. */
class GlimmerSpanCache
{
public:
    GlimmerSpanCache();
    GlimmerSpanCache(const GlimmerSpanCache& other) = delete;
    GlimmerSpanCache& operator=(const GlimmerSpanCache& other) = delete;
    ~GlimmerSpanCache();

    void Update(Frame* frame, const GlimmerChannelState& state, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache);

    HYP_FORCE_INLINE const GlimmerSpanShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    /*! \brief GlimmerSpanValuesPerTexel uints per texel, levels one after the other, texels addressed toroidally. */
    const GpuBufferRef& GetSpansBuffer() const;

private:
    using Rect = GlimmerTexelRect;

    struct RectList
    {
        Rect rects[GlimmerSpanMaxRects];
        uint32 count = 0;

        // false when full
        bool Add(const Rect& rect);
    };

    /*! \brief Clears and refills the spans of the rects (inside the window), leaving the rest of the level as it is. */
    void FillLevel(Frame* frame, uint32 levelIndex, const Vec2i& windowOrigin, const RectList& rects, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache);

    GpuBufferRef m_spansBuffer;

    uint32 m_builtGenerations[GlimmerGroundLevels];
    Vec2i m_builtOrigins[GlimmerGroundLevels];

    // what TLAS swaps changed since the level was last filled, waiting for its turn
    RectList m_changed[GlimmerGroundLevels];
    bool m_changedWholeWindow[GlimmerGroundLevels];
    uint32 m_seenGeneration;

    // filled while the ground under them hadn't loaded, so foliage there wasn't told apart from ground cover; filled again once it has
    RectList m_filledWithoutGround[GlimmerGroundLevels];

    GlimmerSpanShaderData m_shaderData;
};

} // namespace Hyperion
