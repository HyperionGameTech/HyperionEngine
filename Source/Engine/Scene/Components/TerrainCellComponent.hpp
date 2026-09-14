/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class TerrainWorldGridLayer;

HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false)
struct TerrainCellComponent
{
    HYP_STRUCT_BODY(TerrainCellComponent);

    WeakHandle<TerrainWorldGridLayer> layer;

    float lodMorphStart = 0.0f;
    float lodMorphEnd = 0.0f;

    ///the next LOD's morph band is this LOD's band scaled by this
    float lodRangeMultiplier = 1.0f;

    Vec3f lodMorphOrigin;
};

} // namespace Hyperion
