/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/math/Vector2.hpp>

#include <Core/debug/Debug.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct HaltonSequence
{
    static constexpr uint32 size = 128;

    Vec2f sequence[size];

    HaltonSequence()
    {
        for (uint32 index = 0; index < size; index++)
        {
            sequence[index].x = GetHalton(index + 1, 2);
            sequence[index].y = GetHalton(index + 1, 3);
        }
    }

    static inline float GetHalton(uint32 index, uint32 base)
    {
        HYP_CORE_ASSERT(base != 0);

        float f = 1.0f;
        float r = 0.0f;
        uint32 current = index;

        do
        {
            f = f / float(base);
            r = r + f * (current % base);
            current = current / base;
        }
        while (current != 0);

        return r;
    }
};

} // namespace Hyperion
