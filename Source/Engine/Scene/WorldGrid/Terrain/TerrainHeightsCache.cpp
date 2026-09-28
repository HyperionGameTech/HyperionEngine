/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainHeightsCache.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGeneratorState.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

namespace Hyperion {

void TerrainHeightsCache::SampleCache::Invalidate()
{
    HYP_SCOPE;

    heights = Span<const float>();

    scope.Reset();
    cell.Reset();
}

SharedPtr<const Array<float>> TerrainHeightsCache::Find(const Vec2i& coord) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_heightsMutex);

    auto heightsIt = m_heights.Find(coord);

    if (heightsIt != m_heights.End())
    {
        return heightsIt->second;
    }

    return nullptr;
}

SharedPtr<const Array<float>> TerrainHeightsCache::Insert(
    const Vec2i& coord,
    const SharedPtr<Array<float>>& heights,
    const TerrainGeneratorState& generatorState,
    uint32 generationEpoch)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_heightsMutex);

    // the generator was replaced while these were generated - don't poison the new cache
    if (!generatorState.IsCurrent(generationEpoch))
    {
        return heights;
    }

    auto heightsIt = m_heights.Find(coord);

    if (heightsIt != m_heights.End())
    {
        return heightsIt->second;
    }

    m_heights.Set(coord, heights);

    return heights;
}

bool TerrainHeightsCache::TryBeginWarm(const Vec2i& coord)
{
    Mutex::Guard guard(m_warmsMutex);

    if (m_pendingWarms.Find(coord) != m_pendingWarms.End())
    {
        return false;
    }

    m_pendingWarms.Set(coord, true);

    return true;
}

void TerrainHeightsCache::EndWarm(const Vec2i& coord)
{
    Mutex::Guard guard(m_warmsMutex);

    m_pendingWarms.Erase(coord);
}

void TerrainHeightsCache::Clear()
{
    HYP_SCOPE;

    {
        Mutex::Guard guard(m_heightsMutex);

        m_heights.Clear();
    }

    m_sampleCache.Invalidate();
}

} // namespace Hyperion
