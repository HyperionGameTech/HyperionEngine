/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_FILTER_HPP
#define HYPERION_ALGORITHM_FILTER_HPP

#include <core/functional/FunctionWrapper.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

namespace algorithm {

/*! \brief A filter function that applies a predicate to each element in a container.
 *  \param container The container to filter.
 *  \param func The predicate function to apply to each element.
 *  \return A new array containing only the elements that satisfy the predicate. */
template <class ContainerType, class Func>
auto Filter(ContainerType&& container, Func&& func)
{
    using ContainerElementType = typename NormalizedType<ContainerType>::ValueType;
    using PredicateResultType = decltype(std::declval<FunctionWrapper<NormalizedType<Func>>>()(std::declval<ContainerElementType>()));

    FunctionWrapper<NormalizedType<Func>> predicate { std::forward<Func>(func) };

    Array<NormalizedType<ContainerElementType>> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        if (predicate(*it))
        {
            result.PushBack(*it);
        }
    }

    return result;
}

} // namespace algorithm

using algorithm::Filter;

} // namespace hyperion

#endif
