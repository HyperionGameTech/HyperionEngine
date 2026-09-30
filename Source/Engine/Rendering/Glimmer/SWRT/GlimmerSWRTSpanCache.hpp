/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GlimmerTLAS;
class GlimmerBLASCache;
class GlimmerSurfaceCache;

static constexpr uint32 GlimmerSpanValuesPerTexel = 12;

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
    void RebuildLevel(Frame* frame, uint32 levelIndex, const Vec2i& windowOrigin, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache);

    GpuBufferRef m_spansBuffer;

    uint32 m_builtGenerations[GlimmerGroundLevels];
    Vec2i m_builtOrigins[GlimmerGroundLevels];
    bool m_builtWithCompleteGround[GlimmerGroundLevels];

    GlimmerSpanShaderData m_shaderData;
};

} // namespace Hyperion
