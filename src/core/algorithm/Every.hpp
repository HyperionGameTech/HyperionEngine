/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_EVERY_HPP
#define HYPERION_ALGORITHM_EVERY_HPP

#include <core/functional/FunctionWrapper.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace algorithm {

template <class Container, class Predicate>
bool Every(const Container& container, Predicate&& predicate)
{
    FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

    const auto _begin = container.Begin();
    const auto _end = container.End();

    for (auto it = _begin; it != _end; ++it)
    {
        if (!fn(*it))
        {
            return false;
        }
    }

    return true;
}

} // namespace algorithm

using algorithm::Every;

} // namespace hyperion

#endif
