/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/FlatMap.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class TerrainStreamingCell;

/// sim thread only
class TerrainLoadedCells
{
public:
    void Register(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell);

    /// only unregisters if \p cell is the one registered at \p coord
    void Unregister(const Vec2i& coord, const TerrainStreamingCell* cell);

    Handle<TerrainStreamingCell> Find(const Vec2i& coord) const;

    template <class Func>
    void ForEach(Func&& func) const
    {
        for (const KeyValuePair<Vec2i, WeakHandle<TerrainStreamingCell>>& pair : m_cells)
        {
            if (Handle<TerrainStreamingCell> cell = pair.second.Lock(); cell.IsValid())
            {
                func(cell);
            }
        }
    }

    void Clear();

private:
    FlatMap<Vec2i, WeakHandle<TerrainStreamingCell>> m_cells;
};

} // namespace Hyperion
