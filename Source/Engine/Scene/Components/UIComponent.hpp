/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Optional.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Input/Mouse.hpp>
#include <Input/Keyboard.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class UIObject;
class InputManager;

HYP_STRUCT(Component, Size = 8, Serialize = false)
struct UIComponent
{
    HYP_STRUCT_BODY(UIComponent);

    HYP_FIELD()
    WeakHandle<UIObject> uiObject;
};

} // namespace Hyperion
