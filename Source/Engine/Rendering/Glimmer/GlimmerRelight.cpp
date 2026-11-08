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

#include <iterator>

namespace Hyperion {

static constexpr uint32 RelightTexelsPerFrame = 4096;
static constexpr int32 IdleRefreshTexelsPerFrame = 512;
static constexpr size_t MaxPendingRects = 16;

static constexpr size_t MaxEchoes = 64;

#pragma region GlimmerRelight

GlimmerRelight::GlimmerRelight()
    : m_groundGeneration(~0u),
      m_refreshLevel(0),
      m_refreshBudget(0),
      m_hasProbeWindows(false),
      m_frameIndex(0),
      m_shaderData {}
{
}

GlimmerRelight::~GlimmerRelight() = default;

const GpuImageViewRef& GlimmerRelight::GetImageView()
{
    if (!m_texture.IsValid())
    {
        m_texture = GlimmerHelpers::CreateGlimmerStorageTexture(
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
    // out to whole 2x2 blocks, as a block is only lit from its first texel
    const Rect blocks {
        Vec2i(rect.min.x & ~1, rect.min.y & ~1),
        Vec2i((rect.max.x + 1) & ~1, (rect.max.y + 1) & ~1)
    };

    const Rect clipped = Rect::Intersect(
        blocks,
        GlimmerHelpers::GetGlimmerGroundWindow(level.windowOrigin));

    if (clipped.IsEmpty())
    {
        return;
    }

    if (level.pending.Size() >= MaxPendingRects)
    {
        level.pending.Resize(1);
        level.pending[0] = GlimmerHelpers::GetGlimmerGroundWindow(level.windowOrigin);

        return;
    }

    level.pending.PushBack(clipped);
}

void GlimmerRelight::AddEcho(uint32 levelIndex, const Rect& rect)
{
    if (m_echoes.Size() + std::size(GlimmerRelightEchoFrames) > MaxEchoes)
    {
        m_echoes.Clear();

        for (uint32 windowLevel = 0; windowLevel < GlimmerGroundLevels; windowLevel++)
        {
            if (m_levels[windowLevel].hasWindow)
            {
                m_echoes.PushBack(Echo {
                    windowLevel,
                    GlimmerHelpers::GetGlimmerGroundWindow(
                        m_levels[windowLevel].windowOrigin),
                        m_frameIndex + GlimmerRelightEchoFrames[std::size(GlimmerRelightEchoFrames) - 1]
                    });
            }
        }

        return;
    }

    for (uint32 frames : GlimmerRelightEchoFrames)
    {
        m_echoes.PushBack(Echo {
            levelIndex,
            rect,
            m_frameIndex + frames
        });
    }
}

GlimmerTexelRect GlimmerRelight::GetTexelRect(uint32 levelIndex, const Vec2f& min, const Vec2f& max)
{
    const float invTexelSize = 1.0f / GetGlimmerGroundTexelSize(levelIndex);

    return {
        Vec2i(int32(MathUtil::Floor(min.x * invTexelSize)), int32(MathUtil::Floor(min.y * invTexelSize))),
        Vec2i(int32(MathUtil::Ceil(max.x * invTexelSize)), int32(MathUtil::Ceil(max.y * invTexelSize)))
    };
}

void GlimmerRelight::AddChangedArea(const Vec2f& min, const Vec2f& max)
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        if (level.hasWindow)
        {
            AddPending(level, GetTexelRect(levelIndex, min, max));
        }
    }

    AddEchoAround(min, max);
}

void GlimmerRelight::AddEchoAround(const Vec2f& min, const Vec2f& max)
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const Level& level = m_levels[levelIndex];

        if (!level.hasWindow)
        {
            continue;
        }

        const Rect clipped = Rect::Intersect(
            GetTexelRect(levelIndex, min, max),
            GlimmerHelpers::GetGlimmerGroundWindow(level.windowOrigin));

        if (!clipped.IsEmpty())
        {
            AddEcho(levelIndex, clipped);
        }
    }
}

void GlimmerRelight::UpdateProbeWindows(const GlimmerChannelState& state)
{
    if (!state.hasViewer)
    {
        return;
    }

    const int32 window = int32(GlimmerProbeWindow);

    for (uint32 levelIndex = 0; levelIndex < GlimmerProbeLevels; levelIndex++)
    {
        const float blockSize = GlimmerHelpers::GetGlimmerProbeLevelSpacing(levelIndex) * float(GlimmerProbeBlock);

        const Vec3i origin = GlimmerHelpers::GetGlimmerProbeWindowOrigin(state.viewerPosition, state.lookAhead, levelIndex);
        const Vec3i previous = m_probeWindows[levelIndex];

        m_probeWindows[levelIndex] = origin;

        if (!m_hasProbeWindows || origin == previous)
        {
            continue;
        }

        const Vec2f windowMin = Vec2f(float(origin.x) * blockSize, float(origin.z) * blockSize);
        const Vec2f windowMax = Vec2f(float(origin.x + window) * blockSize, float(origin.z + window) * blockSize);

        const int32 dx = origin.x - previous.x;
        const int32 dz = origin.z - previous.z;

        // up or down, or a jump: new probes anywhere under the window
        if (origin.y != previous.y || MathUtil::Abs(dx) >= window || MathUtil::Abs(dz) >= window)
        {
            AddEchoAround(windowMin, windowMax);

            continue;
        }

        if (dx != 0)
        {
            const float edge = float(dx > 0 ? previous.x + window : previous.x) * blockSize;

            AddEchoAround(
                Vec2f(MathUtil::Min(edge, dx > 0 ? windowMax.x : windowMin.x), windowMin.y),
                Vec2f(MathUtil::Max(edge, dx > 0 ? windowMax.x : windowMin.x), windowMax.y));
        }

        if (dz != 0)
        {
            const float edge = float(dz > 0 ? previous.z + window : previous.z) * blockSize;

            AddEchoAround(
                Vec2f(windowMin.x, MathUtil::Min(edge, dz > 0 ? windowMax.y : windowMin.y)),
                Vec2f(windowMax.x, MathUtil::Max(edge, dz > 0 ? windowMax.y : windowMin.y)));
        }
    }

    m_hasProbeWindows = true;
}

void GlimmerRelight::Schedule(
    const GlimmerChannelState& state,
    const GlimmerSurfaceCache& surfaceCache,
    const GlimmerSpanCache& spanCache)
{
    HYP_SCOPE;

    if (state.groundGeneration != m_groundGeneration)
    {
        m_groundGeneration = state.groundGeneration;

        for (Level& level : m_levels)
        {
            level = Level {};
        }

        m_echoes.Clear();
        m_dispatches.Clear();
    }

    // what the last schedule asked for and nothing got to dispatch is still owed
    for (const GlimmerRelightDispatch& dispatch : m_dispatches)
    {
        Level& level = m_levels[dispatch.level];

        if (level.hasWindow)
        {
            AddPending(level, dispatch.rect);
        }
    }

    m_dispatches.Clear();

    int32 budget = RelightTexelsPerFrame;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;
        const Rect window = GlimmerHelpers::GetGlimmerGroundWindow(windowOrigin);

        const bool isJump = !level.hasWindow
            || MathUtil::Abs(windowOrigin.x - level.windowOrigin.x) >= int32(GlimmerGroundResolution)
            || MathUtil::Abs(windowOrigin.y - level.windowOrigin.y) >= int32(GlimmerGroundResolution);

        if (isJump)
        {
            level.hasWindow = true;
            level.windowOrigin = windowOrigin;
            level.isLit = false;
            level.isFilling = true;
            level.refreshRow = window.min.y;
            level.pending.Clear();
            level.pending.PushBack(window);

            AddEcho(levelIndex, window);
        }
        else if (windowOrigin != level.windowOrigin)
        {
            Rect columns;
            Rect rows;

            GlimmerHelpers::GetGlimmerScrolledRects(
                GlimmerHelpers::GetGlimmerGroundWindow(level.windowOrigin),
                window,
                columns,
                rows);

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

    UpdateProbeWindows(state);

    m_frameIndex++;

    for (size_t echoIndex = 0; echoIndex < m_echoes.Size();)
    {
        const Echo echo = m_echoes[echoIndex];

        if (int32(m_frameIndex - echo.frame) < 0)
        {
            echoIndex++;

            continue;
        }

        m_echoes.EraseAt(echoIndex);

        if (m_levels[echo.level].hasWindow)
        {
            AddPending(m_levels[echo.level], echo.rect);
        }
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
            const int32 rows = MathUtil::Min(
                (MathUtil::Max(budget / MathUtil::Max(width, 1), 1) + 1) & ~1,
                rect.max.y - rect.min.y);

            m_dispatches.PushBack(GlimmerRelightDispatch {
                levelIndex,
                Rect {
                    rect.min,
                    Vec2i(rect.max.x, rect.min.y + rows)
                }
            });

            budget -= width * rows;
            rect.min.y += rows;
        }
    }

    const int32 rowTexels = int32(GlimmerGroundResolution);

    m_refreshBudget = MathUtil::Min(
        m_refreshBudget + IdleRefreshTexelsPerFrame,
        MathUtil::Max(IdleRefreshTexelsPerFrame, rowTexels) * 2);

    budget = m_refreshBudget >= rowTexels
        ? m_refreshBudget
        : 0;

    const int32 refreshBudget = budget;

    for (uint32 attempt = 0; attempt < GlimmerGroundLevels && budget > 0; attempt++)
    {
        Level& level = m_levels[m_refreshLevel];

        if (level.pending.Any() || !level.hasWindow)
        {
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;

            continue;
        }

        const Rect window = GlimmerHelpers::GetGlimmerGroundWindow(level.windowOrigin);

        // rows that scrolled out are skipped; the ones that scrolled in were lit as they did
        level.refreshRow = MathUtil::Max(level.refreshRow, window.min.y);

        if (level.refreshRow >= window.max.y)
        {
            level.refreshRow = window.min.y;
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;

            continue;
        }

        level.refreshRow &= ~1;

        const int32 rows = MathUtil::Min(
            (MathUtil::Max(budget / int32(GlimmerGroundResolution), 1) + 1) & ~1,
            window.max.y - level.refreshRow);

        m_dispatches.PushBack(GlimmerRelightDispatch {
            m_refreshLevel,
            Rect {
                Vec2i(window.min.x, level.refreshRow),
                Vec2i(window.max.x, level.refreshRow + rows)
            }
        });

        budget -= rows * int32(GlimmerGroundResolution);

        level.refreshRow += rows;

        if (level.refreshRow >= window.max.y)
        {
            level.refreshRow = window.min.y;
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;
        }

        attempt = 0;
    }

    m_refreshBudget -= refreshBudget - budget;
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

        m_shaderData.levels[levelIndex] = Vec4i(
            level.windowOrigin.x,
            level.windowOrigin.y,
            level.isLit ? 1 : 0,
            0);
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

    m_refreshBudget = 0;

    m_echoes.Clear();
    m_hasProbeWindows = false;
}

#pragma endregion GlimmerRelight

} // namespace Hyperion
