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
#include <Core/Math/Vector4.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Types.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMemory.hpp>

namespace Hyperion {

class TerrainWorldGridLayer;

struct TerrainGroundCoverPaintState
{
    Array<Name, TerrainAllocator> layers;
    Array<ubyte, TerrainAllocator> paint;
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

    void FlattenCircle(const Vec2f& centerXZ, float radius, float falloff, float height);
    void FlattenRect(const Vec2f& minXZ, const Vec2f& maxXZ, float falloff, float height);

    void Ramp(const Vec3f& from, const Vec3f& to, float width, float falloff);

    void Smooth(const Vec2f& centerXZ, float radius, float strength);

    void Paint(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);
    void PaintGroundCover(const Vec3f& worldPos, float radius, float strength, Name groundCoverLayer, bool erase);

    void EndStroke();
    void ResetStroke();

    Array<TerrainGroundCoverPaintEdit, TerrainAllocator> TakeGroundCoverPaintEdits();

private:
    template <bool NeedsSource, class EditFn>
    void EditHeights(const Vec4f& bounds, EditFn&& edit);

    TerrainWorldGridLayer& m_layer;

    FlatMap<Vec2i, bool, TerrainAllocator> m_modifiedCells;

    FlatMap<Vec2i, TerrainGroundCoverPaintState, TerrainAllocator> m_groundCoverStrokeBefore;
    Array<TerrainGroundCoverPaintEdit, TerrainAllocator> m_groundCoverPaintEdits;
};

} // namespace Hyperion
