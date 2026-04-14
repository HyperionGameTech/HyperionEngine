/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/math/Vector3.hpp>

namespace Hyperion {

class CharacterController;
class InputHandlerBase;

HYP_STRUCT(Component,
    Label = "Character Controller Component",
    Description = "Allows an entity to have its movement driven by player input and interact with physics",
    Editor = true)
struct CharacterControllerComponent
{
    HYP_STRUCT_BODY(CharacterControllerComponent);

    HYP_FIELD(Property = "CharacterController")
    Handle<CharacterController> characterController;

    HYP_FIELD(Property = "MoveSpeed")
    float moveSpeed = 5.0f;

    HYP_FIELD(Property = "ViewDirection")
    Vec3f viewDirection = Vec3f(0.0f, 0.0f, 1.0f);

    HYP_FIELD(Transient)
    Handle<InputHandlerBase> inputHandler;
};

} // namespace Hyperion
