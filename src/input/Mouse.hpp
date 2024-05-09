#ifndef HYPERION_MOUSE_HPP
#define HYPERION_MOUSE_HPP

#include <math/Vector2.hpp>
#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

enum class MouseButton : int
{
    INVALID = -1,
    LEFT = 0,
    MIDDLE,
    RIGHT,
};

HYP_MAKE_ENUM_FLAGS(MouseButton)

struct MouseState
{
    EnumFlags<MouseButton>  mask;
    Vec2i                   position;
};

} // namespace hyperion

#endif