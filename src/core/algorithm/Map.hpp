/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_MAP_HPP
#define HYPERION_ALGORITHM_MAP_HPP

#include <core/functional/FunctionWrapper.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

namespace containers {

template <class T, class AllocatorType>
class Array;

} // namespace containers

namespace algorithm {

/*! \brief A map function that applies a function to each element in a container.
 *  \param container The container to map over.
 *  \param func The function to apply to each element.
 *  \return A new array with the results of the function applied to each element. */
template <class ContainerType, class Func>
auto Map(ContainerType &&container, Func &&func)
{
    using ContainerElementType = typename NormalizedType<ContainerType>::ValueType;
    using MapResultType = decltype(std::declval<FunctionWrapper<NormalizedType<Func>>>()(std::declval<ContainerElementType>()));

    FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(func) };

    Array<NormalizedType<MapResultType>> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack(fn(*it));
    }

    return result;
}

} // namespace algorithm

using algorithm::Map;

} // namespace hyperion

#endif
