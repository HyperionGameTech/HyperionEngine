/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_UI_COMPONENT_HPP
#define HYPERION_V2_ECS_UI_COMPONENT_HPP

#include <core/lib/Proc.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Variant.hpp>

#include <core/Name.hpp>
#include <math/Vector2.hpp>

namespace hyperion::v2 {

class UIObject;

struct UIMouseEventData
{
    Vec2f   position;
    int     button = 0;
    bool    is_down = false;
};

struct UIComponent
{
    RC<UIObject>    ui_object;
};

static_assert(sizeof(UIComponent) == 8, "UIComponent should be 8 bytes to match C# struct size");

} // namespace hyperion::v2

#endif