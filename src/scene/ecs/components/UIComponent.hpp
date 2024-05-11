/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/functional/Proc.hpp>
#include <core/containers/String.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/Name.hpp>

#include <input/Mouse.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

class UIObject;
class InputManager;

struct UIMouseEventData
{
    InputManager                *input_manager = nullptr;
    Vec2f                       position;
    Vec2f                       previous_position;
    EnumFlags<MouseButtonState> mouse_buttons = MouseButtonState::NONE;
    bool                        is_down = false;
    Vec2i                       wheel;
};

enum class UIEventHandlerResult : uint32
{
    ERROR           = 0x1u << 31u,
    OK              = 0x0,

    // Stop bubbling the event up the hierarchy
    STOP_BUBBLING   = 0x1
};

HYP_MAKE_ENUM_FLAGS(UIEventHandlerResult)

struct UIComponent
{
    RC<UIObject>    ui_object;
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion

#endif