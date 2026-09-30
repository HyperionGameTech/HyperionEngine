/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GlimmerTLAS;

// Must match GlimmerFootprintMaskParams in Shaders/Glimmer/SWRT/GlimmerSWRTCommon.hlsli
struct GlimmerFootprintMaskShaderData
{
    Vec4f originCellSize; // xy = world xz of cell (0, 0)'s corner, z = cell size, w = 1 when the mask is valid
    Vec4u info;           // x = resolution of level 0, y = number of levels
};

static_assert(sizeof(GlimmerFootprintMaskShaderData) == 32);

static constexpr float GlimmerFootprintMaskCellSize = 1.0f;

/*! \brief 2D grid over the Glimmer region holding the vertical extent of every TLAS instance footprint, plus max-reduced mips.
 *  Covers every instance in the TLAS by construction, so it tells probes and rays when there is nothing in reach for SWRT to hit.
 *  Rebuilt on the GPU whenever a new TLAS is swapped in. Render thread only. */
class GlimmerFootprintMask
{
public:
    GlimmerFootprintMask();
    GlimmerFootprintMask(const GlimmerFootprintMask& other) = delete;
    GlimmerFootprintMask& operator=(const GlimmerFootprintMask& other) = delete;
    ~GlimmerFootprintMask();

    /*! \brief Re-rasterizes every instance of the TLAS into a mask centred on regionCenter (snapped to cells). */
    void Rebuild(Frame* frame, const GlimmerTLAS& tlas, const Vec2f& regionCenterXZ, float regionRadius);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_maskBuffer.IsValid() && m_shaderData.originCellSize.w != 0.0f;
    }

    /*! \brief The mask buffer, or a placeholder until one has been built. */
    const GpuBufferRef& GetMaskBuffer() const;

    HYP_FORCE_INLINE const GlimmerFootprintMaskShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

private:
    void EnsureBuffer(uint32 resolution, uint32 numLevels);

    GpuBufferRef m_maskBuffer;
    GpuBufferRef m_placeholderBuffer;

    uint32 m_bufferResolution;

    GlimmerFootprintMaskShaderData m_shaderData;
};

} // namespace Hyperion
