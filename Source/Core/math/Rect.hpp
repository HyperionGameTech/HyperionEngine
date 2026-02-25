/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/Vector4.hpp>

namespace Hyperion {

template <class T>
struct alignas(8) Rect
{
    static_assert(sizeof(T) == 4, "Rect<T> only supports 32-bit types.");

    union
    {
        struct
        {
            T x0, y0, x1, y1;
        };
    };

    [[nodiscard]]
    HYP_FORCE_INLINE explicit operator Vec4<T>() const
    {
        return { x0, y0, x1, y1 };
    }
};

} // namespace Hyperion
