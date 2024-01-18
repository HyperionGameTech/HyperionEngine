#ifndef HYPERION_MATH_HALTON_HPP
#define HYPERION_MATH_HALTON_HPP

#include <math/Vector2.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion {

struct HaltonSequence
{
    static constexpr UInt size = 128;

    Vec2f sequence[size];

    HaltonSequence()
    {
        for (UInt index = 0; index < size; index++) {
            sequence[index].x = GetHalton(index + 1, 2);
            sequence[index].y = GetHalton(index + 1, 3);
        }
    }

    static inline Float GetHalton(UInt index, UInt base)
    {
        AssertThrow(base != 0);

        Float f = 1.0f;
        Float r = 0.0f;
        UInt current = index;

        do {
            f = f / Float(base);
            r = r + f * (current % base);
            current = current / base;
        } while (current != 0);

        return r;
    }
};

} // namespace hyperion

#endif