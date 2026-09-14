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

class TerrainStreamingCell;

///one drawable piece of a terrain tile's quadtree
HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false)
struct TerrainPatchComponent
{
    HYP_STRUCT_BODY(TerrainPatchComponent);

    WeakHandle<TerrainStreamingCell> cell;

    uint32 patchIndex = 0;
    uint8 level = 0;

    float lodMorphStart = 0.0f;
    float lodMorphEnd = 0.0f;
    float lodRangeMultiplier = 1.0f;
    Vec3f lodMorphOrigin;
};

} // namespace Hyperion
