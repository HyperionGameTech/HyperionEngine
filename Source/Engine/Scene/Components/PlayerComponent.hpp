/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Physics/RigidBody.hpp>

namespace Hyperion {

class InputHandlerBase;

HYP_STRUCT(Component,
    Label = "Player Component",
    Editor = true)
struct PlayerComponent
{
    HYP_STRUCT_BODY(PlayerComponent);
};

} // namespace Hyperion
