/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Optional.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <math/Vector2.hpp>

#include <HashCode.hpp>

namespace hyperion {

class UIObject;
class InputManager;

HYP_STRUCT(Component)
struct UIComponent
{
    HYP_FIELD()
    UIObject    *ui_object = nullptr;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // @TODO
        return HashCode();
    }
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion

#endif