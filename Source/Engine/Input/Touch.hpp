/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

struct TouchEvent;

struct TouchPoint
{
    int32 pointerId = -1;
    Vec2f position;
    Vec2f startPosition;
    bool isActive = false;
    bool isLeftSide = false;  // true = left side (movement), false = right side (look)
};

} // namespace Hyperion
