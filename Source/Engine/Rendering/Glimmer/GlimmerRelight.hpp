/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class GlimmerSurfaceCache;
class GlimmerSpanCache;

/// GLIMMER_RELIGHT_LAYERS
/// what the 2 surfaces receive, 4 parts to a layer
static constexpr uint32 GlimmerRelightLayers = (2 * GlimmerLightParts + 3) / 4;

struct GlimmerRelightShaderData
{
    Vec4i levels[GlimmerGroundLevels]; // xy = absolute texel of the lit window's origin, z = 1 once all of it has been lit
};

struct GlimmerRelightDispatch
{
    uint32 level;
    GlimmerTexelRect rect;
};

class GlimmerRelight final
{
public:
    GlimmerRelight();

    GlimmerRelight(const GlimmerRelight& other) = delete;
    GlimmerRelight& operator=(const GlimmerRelight& other) = delete;

    ~GlimmerRelight();

    void Schedule(
        const GlimmerChannelState& state,
        const GlimmerSurfaceCache& surfaceCache,
        const GlimmerSpanCache& spanCache);

    HYP_FORCE_INLINE const Array<GlimmerRelightDispatch, GlimmerAllocator>& GetDispatches() const
    {
        return m_dispatches;
    }

    void OnDispatched();

    void Invalidate();

    void AddChangedArea(const Vec2f& min, const Vec2f& max);

    HYP_FORCE_INLINE const GlimmerRelightShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    const GpuImageViewRef& GetImageView();
    const GpuImageRef& GetGpuImage() const;

private:
    using Rect = GlimmerTexelRect;

    struct Level
    {
        bool hasWindow = false;
        Vec2i windowOrigin;
        bool isLit = false;
        bool isFilling = false;
        Array<Rect, GlimmerAllocator> pending;
        int32 refreshRow = 0; // absolute texel row the refresh is at
    };

    // what is lit again once the surfels around it have had time to settle
    struct Echo
    {
        uint32 level;
        Rect rect;
        uint32 frame;
    };

    static Rect GetTexelRect(uint32 levelIndex, const Vec2f& min, const Vec2f& max);

    void AddPending(Level& level, const Rect& rect);
    void AddEcho(uint32 levelIndex, const Rect& rect);
    void AddEchoAround(const Vec2f& min, const Vec2f& max);

    Handle<Texture> m_texture;

    FixedArray<Level, GlimmerGroundLevels> m_levels;
    uint32 m_groundGeneration;
    uint32 m_refreshLevel;
    int32 m_refreshBudget;

    Array<Echo, GlimmerAllocator> m_echoes;
    uint32 m_frameIndex;

    Array<GlimmerRelightDispatch, GlimmerAllocator> m_dispatches;

    GlimmerRelightShaderData m_shaderData;
};

} // namespace Hyperion
