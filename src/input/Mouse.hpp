#ifndef HYPERION_MOUSE_HPP
#define HYPERION_MOUSE_HPP

#include <math/Vector2.hpp>
#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

enum class MouseButton : uint32
{
    INVALID = ~0u,
    LEFT = 0,
    MIDDLE,
    RIGHT,
    
    MAX
};

enum class MouseButtonState : uint32
{
    NONE    = 0x0,
    LEFT    = 1u << uint32(MouseButton::LEFT),
    MIDDLE  = 1u << uint32(MouseButton::MIDDLE),
    RIGHT   = 1u << uint32(MouseButton::RIGHT)
};

HYP_MAKE_ENUM_FLAGS(MouseButtonState)

struct MouseState
{
    EnumFlags<MouseButtonState> mask;
    Vec2i                       position;
};

} // namespace hyperion

#endif