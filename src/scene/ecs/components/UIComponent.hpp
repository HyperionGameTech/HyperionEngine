/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/functional/Proc.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/String.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Name.hpp>
#include <math/Vector2.hpp>

namespace hyperion {

class UIObject;

struct UIMouseEventData
{
    Vec2f   position;
    int     button = 0;
    bool    is_down = false;
};

using UIEventHandlerResult = uint32;

enum UIEventHandlerResultBits : UIEventHandlerResult
{
    UI_EVENT_HANDLER_RESULT_ERR             = 0x1u << 31u,
    UI_EVENT_HANDLER_RESULT_OK              = 0x0,

    // Stop bubbling the event up the hierarchy
    UI_EVENT_HANDLER_RESULT_STOP_BUBBLING   = 0x1
};

struct UIComponent
{
    RC<UIObject>    ui_object;
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion

#endif