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

struct GlimmerFootprintMaskShaderData
{
    Vec4f originCellSize; // xy = world xz of cell (0, 0)'s corner, z = cell size, w = 1 when the mask is valid
    Vec4u info;           // x = resolution of level 0, y = number of levels
};

static_assert(sizeof(GlimmerFootprintMaskShaderData) == 32);

static constexpr float GlimmerFootprintMaskCellSize = 1.0f;

class GlimmerFootprintMask final
{
public:
    GlimmerFootprintMask();

    GlimmerFootprintMask(const GlimmerFootprintMask& other) = delete;
    GlimmerFootprintMask& operator=(const GlimmerFootprintMask& other) = delete;

    ~GlimmerFootprintMask();

    void Rebuild(Frame* frame, const GlimmerTLAS& tlas, const Vec2f& regionCenterXZ, float regionRadius);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_maskBuffer.IsValid() && m_shaderData.originCellSize.w != 0.0f;
    }

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
