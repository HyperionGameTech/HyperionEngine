/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <math/Vector2.hpp>

#include <HashCode.hpp>

namespace hyperion {

class UIObject;
class InputManager;

enum class UIEventHandlerResult : uint32
{
    ERR             = 0x1u << 31u,
    OK              = 0x0,

    // Stop bubbling the event up the hierarchy
    STOP_BUBBLING   = 0x1
};

HYP_MAKE_ENUM_FLAGS(UIEventHandlerResult)

struct UIComponent
{
    RC<UIObject>    ui_object;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        // @TODO
        return HashCode();
    }
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion

#endif