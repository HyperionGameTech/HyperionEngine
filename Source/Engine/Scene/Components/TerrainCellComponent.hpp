/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class TerrainWorldGridLayer;
class TerrainStreamingCell;

///on a terrain tile's collider entity - the tile's drawn geometry lives on TerrainPatchComponent entities
HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false)
struct TerrainCellComponent
{
    HYP_STRUCT_BODY(TerrainCellComponent);

    WeakHandle<TerrainWorldGridLayer> layer;
    WeakHandle<TerrainStreamingCell> cell;
};

} // namespace Hyperion
