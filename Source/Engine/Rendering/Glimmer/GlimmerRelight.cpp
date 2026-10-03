/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr int32 RelightTexelsPerFrame = 4096;
static constexpr size_t MaxPendingRects = 16;

#pragma region GlimmerRelight

GlimmerRelight::GlimmerRelight()
    : m_groundGeneration(~0u),
      m_refreshLevel(0),
      m_shaderData {}
{
}

GlimmerRelight::~GlimmerRelight()
{
    m_texture = Handle<Texture>();
}

const GpuImageViewRef& GlimmerRelight::GetImageView()
{
    if (!m_texture.IsValid())
    {
        m_texture = CreateGlimmerStorageTexture(
            TextureType::Texture2DArray,
            TextureFormat::RGBA16F,
            Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1),
            uint16(GlimmerGroundLevels * GlimmerRelightLayers),
            NAME("GlimmerRelight"));
    }

    return RI.textureViewCache->GetOrCreate(m_texture);
}

const GpuImageRef& GlimmerRelight::GetGpuImage() const
{
    AssertDebug(m_texture.IsValid());

    return m_texture->GetGpuImage();
}

void GlimmerRelight::AddPending(Level& level, const Rect& rect)
{
    const Rect clipped = Rect::Intersect(rect, GetGlimmerGroundWindow(level.windowOrigin));

    if (clipped.IsEmpty())
    {
        return;
    }

    if (level.pending.Size() >= MaxPendingRects)
    {
        level.pending.Clear();
        level.pending.PushBack(GetGlimmerGroundWindow(level.windowOrigin));

        return;
    }

    level.pending.PushBack(clipped);
}

void GlimmerRelight::Schedule(const GlimmerChannelState& state, const GlimmerSurfaceCache& surfaceCache, const GlimmerSpanCache& spanCache)
{
    HYP_SCOPE;

    m_dispatches.Clear();

    if (state.groundGeneration != m_groundGeneration)
    {
        m_groundGeneration = state.groundGeneration;

        for (Level& level : m_levels)
        {
            level = Level {};
        }
    }

    int32 budget = RelightTexelsPerFrame;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;
        const Rect window = GetGlimmerGroundWindow(windowOrigin);

        const bool isJump = !level.hasWindow
            || MathUtil::Abs(windowOrigin.x - level.windowOrigin.x) >= int32(GlimmerGroundResolution)
            || MathUtil::Abs(windowOrigin.y - level.windowOrigin.y) >= int32(GlimmerGroundResolution);

        if (isJump)
        {
            level.hasWindow = true;
            level.windowOrigin = windowOrigin;
            level.isLit = false;
            level.isFilling = true;
            level.pending.Clear();
            level.pending.PushBack(window);
        }
        else if (windowOrigin != level.windowOrigin)
        {
            Rect columns;
            Rect rows;
            GetGlimmerScrolledRects(GetGlimmerGroundWindow(level.windowOrigin), window, columns, rows);

            level.windowOrigin = windowOrigin;

            for (Rect& pendingRect : level.pending)
            {
                pendingRect = Rect::Intersect(pendingRect, window);
            }

            for (const Rect& scrolled : { columns, rows })
            {
                if (!scrolled.IsEmpty())
                {
                    m_dispatches.PushBack(GlimmerRelightDispatch { levelIndex, scrolled });
                    budget -= scrolled.Area();
                }
            }
        }

        const Rect& uploadedRect = surfaceCache.GetUploadedRect(levelIndex);

        if (!uploadedRect.IsEmpty())
        {
            AddPending(level, Rect { uploadedRect.min - Vec2i(1, 1), uploadedRect.max + Vec2i(1, 1) });
        }

        AddPending(level, spanCache.GetFilledRect(levelIndex));
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels && budget > 0; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        while (level.pending.Any() && budget > 0)
        {
            Rect& rect = level.pending.Front();

            if (rect.IsEmpty())
            {
                level.pending.PopFront();

                continue;
            }

            const int32 width = rect.max.x - rect.min.x;
            const int32 rows = MathUtil::Clamp(budget / MathUtil::Max(width, 1), 1, rect.max.y - rect.min.y);

            m_dispatches.PushBack(GlimmerRelightDispatch { levelIndex, Rect { rect.min, Vec2i(rect.max.x, rect.min.y + rows) } });

            budget -= width * rows;
            rect.min.y += rows;
        }
    }

    for (uint32 attempt = 0; attempt < GlimmerGroundLevels && budget > 0; attempt++)
    {
        Level& level = m_levels[m_refreshLevel];

        if (level.pending.Any() || !level.hasWindow)
        {
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;

            continue;
        }

        const Rect window = GetGlimmerGroundWindow(level.windowOrigin);
        const int32 rows = MathUtil::Clamp(budget / int32(GlimmerGroundResolution), 1, int32(GlimmerGroundResolution) - level.refreshRow);
        const int32 firstRow = window.min.y + level.refreshRow;

        m_dispatches.PushBack(GlimmerRelightDispatch { m_refreshLevel, Rect { Vec2i(window.min.x, firstRow), Vec2i(window.max.x, firstRow + rows) } });

        budget -= rows * int32(GlimmerGroundResolution);

        level.refreshRow += rows;

        if (level.refreshRow >= int32(GlimmerGroundResolution))
        {
            level.refreshRow = 0;
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;
        }

        attempt = 0;
    }
}

void GlimmerRelight::OnDispatched()
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        if (level.isFilling && level.pending.Empty())
        {
            level.isFilling = false;
            level.isLit = true;
        }

        m_shaderData.levels[levelIndex] = Vec4i(level.windowOrigin.x, level.windowOrigin.y, level.isLit ? 1 : 0, 0);
    }

    m_dispatches.Clear();
}

void GlimmerRelight::Invalidate()
{
    for (Level& level : m_levels)
    {
        level = Level {};
    }

    m_dispatches.Clear();
    m_shaderData = GlimmerRelightShaderData {};
}

#pragma endregion GlimmerRelight

} // namespace Hyperion
