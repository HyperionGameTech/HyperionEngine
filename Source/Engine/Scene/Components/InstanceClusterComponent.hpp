/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class InstanceGroup;

/// Marks a runtime entity that draws one prefab mesh for the instances in one spatial cell of an InstanceGroup.
/// The group owns its instance data.
HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false)
struct InstanceClusterComponent
{
    HYP_STRUCT_BODY(InstanceClusterComponent);

    WeakHandle<InstanceGroup> group;

    Vec2i cell;
    uint32 memberIndex = 0;
};

} // namespace Hyperion
