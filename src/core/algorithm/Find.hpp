/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_FIND_HPP
#define HYPERION_ALGORITHM_FIND_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace algorithm {

template <class IteratorType, class ValueType>
HYP_FORCE_INLINE IteratorType Find(IteratorType _begin, IteratorType _end, ValueType&& value)
{
    for (auto it = _begin; it != _end; ++it)
    {
        if (*it == value)
        {
            return it;
        }
    }

    return _end;
}

} // namespace algorithm

using algorithm::Find;

} // namespace hyperion

#endif
