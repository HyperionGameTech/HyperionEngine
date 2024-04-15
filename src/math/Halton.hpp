/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_MATH_HALTON_HPP
#define HYPERION_MATH_HALTON_HPP

#include <math/Vector2.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion {

struct HaltonSequence
{
    static constexpr uint size = 128;

    Vec2f sequence[size];

    HaltonSequence()
    {
        for (uint index = 0; index < size; index++) {
            sequence[index].x = GetHalton(index + 1, 2);
            sequence[index].y = GetHalton(index + 1, 3);
        }
    }

    static inline float GetHalton(uint index, uint base)
    {
        AssertThrow(base != 0);

        float f = 1.0f;
        float r = 0.0f;
        uint current = index;

        do {
            f = f / float(base);
            r = r + f * (current % base);
            current = current / base;
        } while (current != 0);

        return r;
    }
};

} // namespace hyperion

#endif