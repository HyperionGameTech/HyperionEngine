/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Optional.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/math/Vector2.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class UIObject;
class InputManager;

HYP_STRUCT(Component, Size = 8)
struct UIComponent
{
    HYP_FIELD()
    WeakHandle<UIObject> uiObject;
};

} // namespace hyperion
