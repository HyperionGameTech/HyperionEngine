#ifndef HYPERION_RECT_H
#define HYPERION_RECT_H

#include "Vector2.hpp"
#include "Vector4.hpp"

namespace hyperion {

struct Rect {
    union {
        struct {
            float left,
                right,
                top,
                bottom;
        };

        struct {
            Vector2 coord;
            Vector2 dimension;
        };
    };

    Vector4 ToVector4() const
    {
        return {left, right, top, bottom};
    }
};

} // namespace hyperion

#endif
