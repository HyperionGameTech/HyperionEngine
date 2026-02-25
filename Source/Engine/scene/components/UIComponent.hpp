/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Optional.hpp>

#include <Core/reflection/Handle.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

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
