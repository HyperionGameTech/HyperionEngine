/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ALGORITHM_FIND_IF_HPP
#define HYPERION_ALGORITHM_FIND_IF_HPP

#include <core/Defines.hpp>

#include <core/functional/FunctionWrapper.hpp>

#include <Types.hpp>

namespace hyperion {
namespace algorithm {

template <class IteratorType, class Predicate>
IteratorType FindIf(IteratorType begin, IteratorType end, Predicate&& predicate)
{
    FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

    for (auto it = begin; it != end; ++it)
    {
        if (fn(*it))
        {
            return it;
        }
    }

    return end;
}

template <class ContainerType, class Predicate>
typename ContainerType::Iterator FindIf(ContainerType& container, Predicate&& predicate)
{
    FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

    typename ContainerType::Iterator begin = container.Begin();
    typename ContainerType::Iterator end = container.End();

    for (auto it = begin; it != end; ++it)
    {
        if (fn(*it))
        {
            return it;
        }
    }

    return end;
}

template <class ContainerType, class Predicate>
typename ContainerType::ConstIterator FindIf(const ContainerType& container, Predicate&& predicate)
{
    FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

    typename ContainerType::ConstIterator begin = container.Begin();
    typename ContainerType::ConstIterator end = container.End();

    for (auto it = begin; it != end; ++it)
    {
        if (fn(*it))
        {
            return it;
        }
    }

    return end;
}

} // namespace algorithm

using algorithm::FindIf;

} // namespace hyperion

#endif
