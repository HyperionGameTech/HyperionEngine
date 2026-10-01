/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerGroundClipmap.hpp>

#include <Scene/World.hpp>
#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr int32 WindowSnapTexels = 16;
static constexpr int32 RefreshRows = 8;
static constexpr int32 SamplesPerFrame = 65536;

#pragma region GlimmerGroundClipmap

GlimmerGroundClipmap::GlimmerGroundClipmap()
    : m_terrain(nullptr),
      m_generation(0)
{
}

void GlimmerGroundClipmap::MoveWindow(uint32 levelIndex, const Vec2i& desiredOrigin)
{
    Level& level = m_levels[levelIndex];

    if (level.hasWindow && level.windowOrigin == desiredOrigin)
    {
        return;
    }

    const Rect newWindow = GetGlimmerGroundWindow(desiredOrigin);

    const bool isJump = !level.hasWindow
        || MathUtil::Abs(desiredOrigin.x - level.windowOrigin.x) >= int32(GlimmerGroundResolution)
        || MathUtil::Abs(desiredOrigin.y - level.windowOrigin.y) >= int32(GlimmerGroundResolution);

    if (isJump)
    {
        level.valid = Rect { desiredOrigin, desiredOrigin };
        level.pending.Clear();
        level.pending.PushBack(newWindow);
    }
    else
    {
        const Rect oldWindow = GetGlimmerGroundWindow(level.windowOrigin);

        level.valid = Rect::Intersect(level.valid, newWindow);

        for (Rect& pendingRect : level.pending)
        {
            pendingRect = Rect::Intersect(pendingRect, newWindow);
        }

        Rect columns;
        Rect rows;
        GetGlimmerScrolledRects(oldWindow, newWindow, columns, rows);

        if (!columns.IsEmpty())
        {
            level.pending.PushBack(columns);
        }

        if (!rows.IsEmpty())
        {
            level.pending.PushBack(rows);
        }
    }

    level.windowOrigin = desiredOrigin;
    level.hasWindow = true;
}

int32 GlimmerGroundClipmap::SampleRect(TerrainWorldGridLayer* terrain, uint32 levelIndex, const Rect& rect, Array<GlimmerGroundUpload>& outUploads) const
{
    HYP_SCOPE;

    if (rect.IsEmpty())
    {
        return 0;
    }

    const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

    const bool supersample = levelIndex != 0;

    GlimmerGroundUpload& upload = outUploads.EmplaceBack();
    upload.level = levelIndex;
    upload.texelMin = rect.min;
    upload.extent = Vec2u(uint32(rect.max.x - rect.min.x), uint32(rect.max.y - rect.min.y));
    upload.heights.Resize(size_t(upload.extent.x) * size_t(upload.extent.y));

    for (int32 z = rect.min.y; z < rect.max.y; z++)
    {
        for (int32 x = rect.min.x; x < rect.max.x; x++)
        {
            const Vec2f center = Vec2f((float(x) + 0.5f) * texelSize, (float(z) + 0.5f) * texelSize);

            float height;

            if (supersample)
            {
                const float offset = 0.25f * texelSize;

                height = 0.25f * (terrain->SampleHeightAt(center + Vec2f(-offset, -offset))
                    + terrain->SampleHeightAt(center + Vec2f(offset, -offset))
                    + terrain->SampleHeightAt(center + Vec2f(-offset, offset))
                    + terrain->SampleHeightAt(center + Vec2f(offset, offset)));
            }
            else
            {
                height = terrain->SampleHeightAt(center);
            }

            upload.heights[size_t(z - rect.min.y) * upload.extent.x + size_t(x - rect.min.x)] = height;
        }
    }

    return rect.Area() * (supersample ? 4 : 1);
}

void GlimmerGroundClipmap::Update(World* world, const Vec3f& viewerPosition, Array<GlimmerGroundUpload>& outUploads)
{
    HYP_SCOPE;

    TerrainWorldGridLayer* terrain = nullptr;

    if (world && world->GetWorldGrid().IsValid())
    {
        for (const Handle<WorldGridLayer>& layer : world->GetWorldGrid()->GetLayers())
        {
            if (const Handle<TerrainWorldGridLayer>& terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer); terrainLayer.IsValid())
            {
                terrain = terrainLayer.Get();

                break;
            }
        }
    }

    if (terrain != m_terrain)
    {
        m_terrain = terrain;
        m_generation++;

        for (Level& level : m_levels)
        {
            level = Level {};
        }
    }

    if (!m_terrain)
    {
        return;
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);
        const int32 half = int32(GlimmerGroundResolution / 2);

        const Vec2i desiredOrigin = Vec2i(
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(viewerPosition.x / texelSize)) - half, WindowSnapTexels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(viewerPosition.z / texelSize)) - half, WindowSnapTexels));

        MoveWindow(levelIndex, desiredOrigin);
    }

    int32 budget = SamplesPerFrame;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels && budget > 0; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        const int32 samplesPerTexel = levelIndex != 0 ? 4 : 1;

        while (level.pending.Any() && budget > 0)
        {
            Rect& rect = level.pending.Front();

            if (rect.IsEmpty())
            {
                level.pending.PopFront();

                continue;
            }

            const int32 width = rect.max.x - rect.min.x;
            const int32 rows = MathUtil::Clamp(budget / MathUtil::Max(width * samplesPerTexel, 1), 1, rect.max.y - rect.min.y);

            const Rect chunk = Rect { rect.min, Vec2i(rect.max.x, rect.min.y + rows) };

            budget -= SampleRect(m_terrain, levelIndex, chunk, outUploads);

            rect.min.y += rows;
        }

        if (level.pending.Empty())
        {
            level.valid = GetGlimmerGroundWindow(level.windowOrigin);
        }
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels && budget > 0; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        if (level.pending.Any())
        {
            continue;
        }

        const Rect window = GetGlimmerGroundWindow(level.windowOrigin);

        const int32 row = window.min.y + (level.refreshRow % int32(GlimmerGroundResolution));
        const Rect strip = Rect { Vec2i(window.min.x, row), Vec2i(window.max.x, MathUtil::Min(row + RefreshRows, window.max.y)) };

        budget -= SampleRect(m_terrain, levelIndex, strip, outUploads);

        level.refreshRow = (level.refreshRow + RefreshRows) % int32(GlimmerGroundResolution);
    }
}

void GlimmerGroundClipmap::FillState(GlimmerChannelState& outState) const
{
    outState.groundGeneration = m_generation;

    if (m_terrain)
    {
        FillGlimmerGroundCover(m_terrain, outState);
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const Level& level = m_levels[levelIndex];

        GlimmerGroundLevelState& levelState = outState.groundLevels[levelIndex];
        levelState.windowOrigin = level.windowOrigin;

        if (m_terrain && level.hasWindow)
        {
            levelState.validMin = level.valid.min;
            levelState.validMax = level.valid.max;
        }
        else
        {
            levelState.validMin = Vec2i(0, 0);
            levelState.validMax = Vec2i(0, 0);
        }
    }
}

#pragma endregion GlimmerGroundClipmap

} // namespace Hyperion
