/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Optional.hpp>

#include <Core/reflection/Handle.hpp>

#include <Input/Mouse.hpp>
#include <Input/Keyboard.hpp>

#include <Core/math/Vector2.hpp>

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
