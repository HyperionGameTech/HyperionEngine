/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class TerrainWorldGridLayer;

struct TerrainGroundCoverPaintState
{
    Array<Name> layers;
    Array<ubyte> paint;
};

struct TerrainGroundCoverPaintEdit
{
    Vec2i coord;
    TerrainGroundCoverPaintState before;
    TerrainGroundCoverPaintState after;
};

/// sim thread only - sculpt and paint strokes on a terrain layer's cells
class TerrainBrush
{
public:
    explicit TerrainBrush(TerrainWorldGridLayer& layer);

    TerrainBrush(const TerrainBrush& other) = delete;
    TerrainBrush& operator=(const TerrainBrush& other) = delete;

    void Sculpt(const Vec3f& worldPos, float radius, float strength, bool raise);
    void Paint(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);

    /// paints the painted GroundCover layer \p groundCoverLayer. Painting one takes the ground from the others
    void PaintGroundCover(const Vec3f& worldPos, float radius, float strength, Name groundCoverLayer, bool erase);

    void EndStroke();

    /// forgets the cells touched by the current stroke without finishing it
    void ResetStroke();

    Array<TerrainGroundCoverPaintEdit> TakeGroundCoverPaintEdits();

private:
    TerrainWorldGridLayer& m_layer;

    FlatMap<Vec2i, bool> m_modifiedCells;

    FlatMap<Vec2i, TerrainGroundCoverPaintState> m_groundCoverStrokeBefore;
    Array<TerrainGroundCoverPaintEdit> m_groundCoverPaintEdits;
};

} // namespace Hyperion
