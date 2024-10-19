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

struct alignas(8) UIEventHandlerResult
{
    enum Value : uint32
    {
        ERR             = 0x1u << 31u,
        OK              = 0x0,

        // Stop bubbling the event up the hierarchy
        STOP_BUBBLING   = 0x1
    };

    UIEventHandlerResult()
        : value(OK),
          message(nullptr)
    {
    }

    UIEventHandlerResult(uint32 value)
        : value(value),
          message(nullptr)
    {
    }

    template <class StaticMessageType>
    UIEventHandlerResult(uint32 value, StaticMessageType)
        : value(value),
          message(StaticMessageType::value.Data())
    {
    }

    UIEventHandlerResult &operator=(uint32 value)
    {
        this->value = value;
        this->message = nullptr;

        return *this;
    }

    UIEventHandlerResult(const UIEventHandlerResult &other)                 = default;
    UIEventHandlerResult &operator=(const UIEventHandlerResult &other)      = default;

    UIEventHandlerResult(UIEventHandlerResult &&other) noexcept             = default;
    UIEventHandlerResult &operator=(UIEventHandlerResult &&other) noexcept  = default;

    ~UIEventHandlerResult()                                                 = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return value != 0; }

    HYP_FORCE_INLINE bool operator!() const
        { return !value; }

    HYP_FORCE_INLINE explicit operator uint32() const
        { return value; }

    HYP_FORCE_INLINE bool operator==(const UIEventHandlerResult &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const UIEventHandlerResult &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE UIEventHandlerResult operator&(const UIEventHandlerResult &other) const
        { return UIEventHandlerResult(value & other.value); }

    HYP_FORCE_INLINE UIEventHandlerResult &operator&=(const UIEventHandlerResult &other)
    {
        value &= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator|(const UIEventHandlerResult &other) const
        { return UIEventHandlerResult(value | other.value); }

    HYP_FORCE_INLINE UIEventHandlerResult &operator|=(const UIEventHandlerResult &other)
    {
        value |= other.value;

        return *this;
    }

    HYP_FORCE_INLINE UIEventHandlerResult operator~() const
        { return UIEventHandlerResult(~value); }

    HYP_FORCE_INLINE Optional<ANSIStringView> GetMessage() const
    {
        if (!message) {
            return { };
        }

        return ANSIStringView(message);
    }

    uint32      value;
    const char  *message;
};

static_assert(sizeof(UIEventHandlerResult) == 16, "sizeof(UIEventHandlerResult) must match C# struct size");
static_assert(offsetof(UIEventHandlerResult, message) == 8, "offsetof(UIEventHandlerResult, message) expected to return 8");

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