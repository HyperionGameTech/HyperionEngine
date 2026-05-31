/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/Memory.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/functional/FunctionWrapper.hpp>

#include <Core/Defines.hpp>

#include <Core/Constants.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <algorithm>

namespace Hyperion {

template <class ValueType>
constexpr decltype(auto) KeyBy_Identity(const ValueType& value)
{
    return value;
}

namespace containers {

/*! \brief Interface used by all container types. Used for type traits and static assertions. */
class IContainer
{
public:
};

/*! \brief Base class for all container types.
 *  \tparam Container The derived container type.
 *  \tparam Key The key type used for searching and indexing.
 *
 *  Provides common functionality for all container types. Provides some shared functionality such as Find, FindIf, Contains, etc.
 */
template <class Container, class Key>
class ContainerBase : public IContainer
{
protected:
    using Base = ContainerBase;

public:
    using KeyType = Key;

    ContainerBase()
    {
    }

    ~ContainerBase()
    {
    }

    template <class T>
    auto Find(const T& value)
    {
        return std::find(
            static_cast<Container*>(this)->Begin(),
            static_cast<Container*>(this)->End(),
            value);
    }

    template <class T>
    auto Find(const T& value) const
    {
        return std::find(
            static_cast<const Container*>(this)->Begin(),
            static_cast<const Container*>(this)->End(),
            value);
    }

    template <class U>
    auto FindAs(const U& value)
    {
        return std::find_if(
            static_cast<Container*>(this)->Begin(),
            static_cast<Container*>(this)->End(),
            [&value](const auto& otherValue)
            {
                return otherValue == value;
            });
    }

    template <class U>
    auto FindAs(const U& value) const
    {
        return std::find_if(
            static_cast<const Container*>(this)->Begin(),
            static_cast<const Container*>(this)->End(),
            [&value](const auto& otherValue)
            {
                return otherValue == value;
            });
    }

    template <class Func>
    auto FindIf(Func&& pred)
    {
        return std::find_if(
            static_cast<Container*>(this)->Begin(),
            static_cast<Container*>(this)->End(),
            FunctionWrapper<NormalizedType<Func>> { std::forward<Func>(pred) });
    }

    template <class Func>
    auto FindIf(Func&& pred) const
    {
        FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(pred) };

        return std::find_if(
            static_cast<const Container*>(this)->Begin(),
            static_cast<const Container*>(this)->End(),
            FunctionWrapper<NormalizedType<Func>> { std::forward<Func>(pred) });
    }

    template <class T>
    auto LowerBound(const T& key)
    {
        static_assert(Container::isContiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        return std::lower_bound(_begin, _end, key);
    }

    template <class T>
    auto LowerBound(const T& key) const
    {
        static_assert(Container::isContiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        return std::lower_bound(_begin, _end, key);
    }

    template <class T>
    auto UpperBound(const T& key)
    {
        static_assert(Container::isContiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        return std::upper_bound(_begin, _end, key);
    }

    template <class T>
    auto UpperBound(const T& key) const
    {
        static_assert(Container::isContiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        return std::upper_bound(_begin, _end, key);
    }

    template <class T>
    bool Contains(const T& value) const
    {
        return static_cast<const Container*>(this)->Find(value)
            != static_cast<const Container*>(this)->End();
    }

    /*! \brief Returns the number of elements matching the given value. */
    template <class T>
    size_t Count(const T& value) const
    {
        size_t count = 0;

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        for (auto it = _begin; it != _end; ++it)
        {
            if (*it == value)
            {
                ++count;
            }
        }

        return count;
    }

    auto Sum() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container*>(this)->Begin())>>;

        HeldType result {};
        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist)
        {
            return result;
        }

        for (auto it = _begin; it != _end; ++it)
        {
            result += static_cast<HeldType>(*it);
        }

        return result;
    }

    auto Avg() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container*>(this)->Begin())>>;

        HeldType result {};

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist)
        {
            return result;
        }

        for (auto it = _begin; it != _end; ++it)
        {
            result += static_cast<HeldType>(*it);
        }

        result /= dist;

        return result;
    }

    template <class Predicate>
    void RemoveAll(Predicate&& predicate)
    {
        FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

        auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        while (_begin != _end)
        {
            if (fn(*_begin))
            {
                _begin = static_cast<Container*>(this)->Erase(_begin);
            }
            else
            {
                ++_begin;
            }
        }
    }

    template <class ConstIterator>
    size_t IndexOf(ConstIterator iter) const
    {
        static_assert(Container::isContiguous, "Container must be contiguous to perform IndexOf()");

        static_assert(std::is_convertible_v<decltype(iter),
                          typename Container::ConstIterator>,
            "Iterator type does not match container");

        return iter != static_cast<const Container*>(this)->End()
            ? size_t(iter - static_cast<const Container*>(this)->Begin())
            : size_t(-1);
    }

    template <class OtherContainer>
    bool CompareBitwise(const OtherContainer& otherContainer) const
    {
        static_assert(Container::isContiguous && OtherContainer::isContiguous, "Containers must be contiguous to perform bitwise comparison");

        const size_t thisSizeBytes = static_cast<const Container*>(this)->ByteSize();
        const size_t otherSizeBytes = otherContainer.ByteSize();

        if (thisSizeBytes != otherSizeBytes)
        {
            return false;
        }

        return Memory::Compare(
                   static_cast<const Container*>(this)->Begin(),
                   otherContainer.Begin(),
                   thisSizeBytes)
            == 0;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (auto it = static_cast<const Container*>(this)->Begin(); it != static_cast<const Container*>(this)->End(); ++it)
        {
            hc.Add(*it);
        }

        return hc;
    }
};

template <class IteratorType, class ValueType>
static inline void Fill(IteratorType begin, IteratorType end, const ValueType& value)
{
    for (auto it = begin; it != end; ++it)
    {
        *begin = value;
    }
}

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

template <class Container, class Predicate>
bool AnyOf(const Container& container, Predicate&& predicate)
{
    FunctionWrapper<NormalizedType<Predicate>> fn { std::forward<Predicate>(predicate) };

    const auto _begin = container.Begin();
    const auto _end = container.End();

    for (auto it = _begin; it != _end; ++it)
    {
        if (fn(*it))
        {
            return true;
        }
    }

    return false;
}

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

} // namespace containers

using containers::AnyOf;
using containers::ContainerBase;
using containers::Every;
using containers::Fill;
using containers::Find;
using containers::FindIf;
using containers::IContainer;
using containers::Sum;

} // namespace Hyperion
