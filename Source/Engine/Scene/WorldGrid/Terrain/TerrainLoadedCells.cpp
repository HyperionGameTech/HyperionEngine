/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainLoadedCells.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

void TerrainLoadedCells::Register(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell)
{
    AssertOnThread(g_simThread);

    m_cells[coord] = cell;
}

void TerrainLoadedCells::Unregister(const Vec2i& coord, const TerrainStreamingCell* cell)
{
    AssertOnThread(g_simThread);

    auto cellIt = m_cells.Find(coord);

    if (cellIt == m_cells.End() || cellIt->second.GetUnsafe() != cell)
    {
        return;
    }

    m_cells.Erase(cellIt);
}

Handle<TerrainStreamingCell> TerrainLoadedCells::Find(const Vec2i& coord) const
{
    AssertOnThread(g_simThread);

    auto cellIt = m_cells.Find(coord);

    if (cellIt == m_cells.End())
    {
        return Handle<TerrainStreamingCell>();
    }

    return cellIt->second.Lock();
}

void TerrainLoadedCells::Clear()
{
    AssertOnThread(g_simThread);

    m_cells.Clear();
}

} // namespace Hyperion
