/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false)
struct TerrainGrassComponent
{
    HYP_STRUCT_BODY(TerrainGrassComponent);

    float spawnTime = 0.0f;
};

} // namespace Hyperion
