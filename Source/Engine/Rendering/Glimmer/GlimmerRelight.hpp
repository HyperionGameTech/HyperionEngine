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

static constexpr uint32 GlimmerRelightLayers = 2;

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
        const GlimmerSpanCache& spanCache,
        bool wakeLighting);

    HYP_FORCE_INLINE const Array<GlimmerRelightDispatch>& GetDispatches() const
    {
        return m_dispatches;
    }

    void OnDispatched();

    void Invalidate();

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
        Array<Rect> pending;
        int32 refreshRow = 0;
    };

    void AddPending(Level& level, const Rect& rect);

    Handle<Texture> m_texture;

    FixedArray<Level, GlimmerGroundLevels> m_levels;
    uint32 m_groundGeneration;
    uint32 m_refreshLevel;
    int32 m_refreshBudget;
    int32 m_burstTexelsLeft;

    Array<GlimmerRelightDispatch> m_dispatches;

    GlimmerRelightShaderData m_shaderData;
};

} // namespace Hyperion
