#ifndef HYPERION_UTIL_MISCUTIL_HPP
#define HYPERION_UTIL_MISCUTIL_HPP

#include <util/Defines.hpp>

#include <algorithm>

#include <math/Extent.hpp>
#include <core/lib/DynArray.hpp>

namespace hyperion {

template <class T>
static Array<FixedArray<T, 2>> FindFactors(T num)
{
    Array<FixedArray<T, 2>> factors;

    for (T i = 1; i <= num; i++) {
        if (num % i == 0) {
            factors.PushBack(FixedArray<T, 2> { i, num / i });
        }
    }

    return factors;
}

static inline Extent2D ReshapeExtent(Extent3D extent)
{
    auto factors = FindFactors(extent.Size());

    // Sort it so lowest difference is front
    std::sort(
        factors.Begin(),
        factors.End(),
        [](auto a, auto b)
        {
            const UInt a_diff = MathUtil::Abs(a[0] - a[1]);
            const UInt b_diff = MathUtil::Abs(b[0] - b[1]);

            return a_diff < b_diff;
        }
    );

    if (factors.Size() == 0) {
        return Extent2D { 0, 0 };
    }

    const auto most_balanced_pair = factors[0];

    return Extent2D { UInt(most_balanced_pair[0]), UInt(most_balanced_pair[1]) };
}

} // namespace hyperion

#endif