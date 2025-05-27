/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_COMPONENT_HPP
#define HYPERION_ECS_UI_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Optional.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/math/Vector2.hpp>

#include <HashCode.hpp>

namespace hyperion {

class UIObject;
class InputManager;

HYP_STRUCT(Component, Size = 8)

struct UIComponent
{
    HYP_FIELD()
    UIObject* ui_object = nullptr;
};

} // namespace hyperion

#endif