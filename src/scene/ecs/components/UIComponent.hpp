/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/functional/Proc.hpp>
#include <core/containers/String.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/Name.hpp>

#include <input/Mouse.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

class UIObject;

struct UIMouseEventData
{
    Vec2f       position;
    MouseButton button = MouseButton::INVALID;
    bool        is_down = false;
    Vec2i       wheel;
};

using UIEventHandlerResult = uint32;

enum UIEventHandlerResultBits : UIEventHandlerResult
{
    UEHR_ERR            = 0x1u << 31u,
    UEHR_OK             = 0x0,

    // Stop bubbling the event up the hierarchy
    UEHR_STOP_BUBBLING  = 0x1
};

struct UIComponent
{
    RC<UIObject>    ui_object;
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion

#endif