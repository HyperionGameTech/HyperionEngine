/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_SUM_HPP
#define HYPERION_ALGORITHM_SUM_HPP

#include <core/functional/FunctionWrapper.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace algorithm {

/*! \brief A sum function that computes the total of all elements in a container.
 *  \param container The container to sum over.
 *  \return The total sum of the container's elements.
 */
template <class ContainerType, class Func>
auto Sum(ContainerType&& container, Func&& func)
{
    using ContainerElementType = typename NormalizedType<ContainerType>::ValueType;
    using SumResultType = decltype(std::declval<FunctionWrapper<NormalizedType<Func>>>()(std::declval<ContainerElementType>()));

    FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(func) };

    SumResultType total = SumResultType(0);

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        total += fn(*it);
    }

    return total;
}

} // namespace algorithm

using algorithm::Sum;

} // namespace hyperion

#endif
