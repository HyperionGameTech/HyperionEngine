/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONTAINER_BASE_HPP
#define HYPERION_CONTAINER_BASE_HPP

#include <core/memory/Memory.hpp>

#include <core/Defines.hpp>

#include <Constants.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <algorithm>

namespace hyperion {

template <auto KeyByFunction>
struct KeyByFunctionInvoker;

template <class TargetType, class ReturnType, ReturnType(TargetType::*func)()>
struct KeyByFunctionInvoker<func>
{
    using Type = decltype(func);

    constexpr decltype(auto) operator()(TargetType &value) const
    {
        return (value.*func)();
    }

    constexpr decltype(auto) operator()(const TargetType &value) const
    {
        return (value.*func)();
    }
};

template <class TargetType, class ReturnType, ReturnType(TargetType::*func)() const>
struct KeyByFunctionInvoker<func>
{
    using Type = decltype(func);

    constexpr decltype(auto) operator()(const TargetType &value) const
    {
        return (value.*func)();
    }
};

template <class TargetType, class ReturnType, ReturnType TargetType::*member>
struct KeyByFunctionInvoker<member>
{
    using Type = decltype(member);

    constexpr decltype(auto) operator()(TargetType &value) const
    {
        return value.*member;
    }

    constexpr decltype(auto) operator()(const TargetType &value) const
    {
        return value.*member;
    }
};

template <class TargetType, class ReturnType, ReturnType(*func)(TargetType &)>
struct KeyByFunctionInvoker<func>
{
    using Type = decltype(func);

    constexpr decltype(auto) operator()(TargetType &value) const
    {
        return func(value);
    }
};

template <class TargetType, class ReturnType, ReturnType(*func)(const TargetType &)>
struct KeyByFunctionInvoker<func>
{
    using Type = decltype(func);

    constexpr decltype(auto) operator()(const TargetType &value) const
    {
        return func(value);
    }
};

template <class ValueType>
constexpr decltype(auto) KeyBy_Identity(const ValueType &value)
{
    return value;
}

template <class Container, class Key>
class ContainerBase
{
protected:
    using Base = ContainerBase;
public:
    using KeyType = Key;

    ContainerBase() { }
    ~ContainerBase() { }

    auto &Get(KeyType key)
    {
        return static_cast<Container *>(this)->operator[](key);
    }

    const auto &Get(KeyType key) const
    {
        const auto it = static_cast<const Container *>(this)->Find(key);
        AssertThrowMsg(it != static_cast<const Container *>(this)->End(), "Cannot Get(): value not found");

        return *it;
    }

    auto *TryGet(KeyType key)
    {
        const auto it = static_cast<Container *>(this)->Find(key);
        return it != static_cast<Container *>(this)->End()
            ? &(*it)
            : nullptr;
    }

    const auto *TryGet(KeyType key) const
    {
        const auto it = static_cast<const Container *>(this)->Find(key);
        return it != static_cast<const Container *>(this)->End()
            ? &(*it)
            : nullptr;
    }

    template <class ValueType>
    void Set(KeyType index, const ValueType &value)
    {
        AssertThrow(index < static_cast<KeyType>(static_cast<const Container *>(this)->Size()));
        static_cast<Container *>(this)->operator[](index) = value;
    }
    
    template <class ValueType>
    void Set(KeyType index, ValueType &&value)
    {
        AssertThrow(index < static_cast<KeyType>(static_cast<const Container *>(this)->Size()));
        static_cast<Container *>(this)->operator[](index) = std::forward<ValueType>(value);
    }

    template <class T>
    auto Find(const T &value)
    {
        auto _begin = static_cast<Container *>(this)->Begin();
        const auto _end = static_cast<Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (*_begin == value) {
                return _begin;
            }
        }

        return _begin;
    }
    
    template <class T>
    auto Find(const T &value) const
    {
        auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (*_begin == value) {
                return _begin;
            }
        }

        return _begin;
    }

    template <class U>
    auto FindAs(const U &value)
    {
        auto _begin = static_cast<Container *>(this)->Begin();
        const auto _end = static_cast<Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (value == *_begin) {
                return _begin;
            }
        }

        return _begin;
    }

    template <class U>
    auto FindAs(const U &value) const
    {
        auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (value == *_begin) {
                return _begin;
            }
        }

        return _begin;
    }

    template <class Function>
    auto FindIf(Function &&pred)
    {
        typename Container::Iterator _begin = static_cast<Container *>(this)->Begin();
        const typename Container::Iterator _end = static_cast<Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (pred(*_begin)) {
                return _begin;
            }
        }

        return _begin;
    }

    template <class Function>
    auto FindIf(Function &&pred) const
    {
        typename Container::ConstIterator _begin = static_cast<const Container *>(this)->Begin();
        const typename Container::ConstIterator _end = static_cast<const Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (pred(*_begin)) {
                return _begin;
            }
        }

        return _begin;
    }
    
    template <class T>
    auto LowerBound(const T &key)
    {
        const auto _begin = static_cast<Container *>(this)->Begin();
        const auto _end = static_cast<Container *>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key
        );
    }
    
    template <class T>
    auto LowerBound(const T &key) const
    {
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key
        );
    }
    
    template <class T>
    auto UpperBound(const T &key)
    {
        const auto _begin = static_cast<Container *>(this)->Begin();
        const auto _end = static_cast<Container *>(this)->End();

        return std::upper_bound(
            _begin,
            _end,
            key
        );
    }
    
    template <class T>
    auto UpperBound(const T &key) const
    {
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        return std::upper_bound(
            _begin,
            _end,
            key
        );
    }

    template <class T>
    bool Contains(const T &value) const
    {
        return static_cast<const Container *>(this)->Find(value)
            != static_cast<const Container *>(this)->End();
    }

    template <class Lambda>
    bool Any(Lambda &&lambda) const
    {
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        for (auto it = _begin; it != _end; ++it) {
            if (lambda(*it)) {
                return true;
            }
        }

        return false;
    }

    template <class Lambda>
    bool Every(Lambda &&lambda) const
    {
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        for (auto it = _begin; it != _end; ++it) {
            if (!lambda(*it)) {
                return false;
            }
        }

        return true;
    }

    auto Sum() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container *>(this)->Begin())>>;

        HeldType result { };
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist) {
            return result;
        }

        for (auto it = _begin; it != _end; ++it) {
            result += static_cast<HeldType>(*it);
        }

        return result;
    }

    auto Avg() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container *>(this)->Begin())>>;

        HeldType result { };

        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist) {
            return result;
        }

        for (auto it = _begin; it != _end; ++it) {
            result += static_cast<HeldType>(*it);
        }

        result /= dist;

        return result;
    }

    template <class ConstIterator>
    KeyType IndexOf(ConstIterator iter) const
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        static_assert(std::is_convertible_v<decltype(iter),
            typename Container::ConstIterator>, "Iterator type does not match container");

        return iter != static_cast<const Container *>(this)->End()
            ? static_cast<KeyType>(std::distance(static_cast<const Container *>(this)->Begin(), iter))
            : static_cast<KeyType>(-1);
    }

    template <class OtherContainer>
    bool CompareBitwise(const OtherContainer &other_container) const
    {
        static_assert(Container::is_contiguous && OtherContainer::is_contiguous, "Containers must be contiguous to perform bitwise comparison");

        const SizeType this_size_bytes = static_cast<const Container *>(this)->ByteSize();
        const SizeType other_size_bytes = other_container.ByteSize();

        if (this_size_bytes != other_size_bytes) {
            return false;
        }

        return Memory::MemCmp(
            static_cast<const Container *>(this)->Begin(),
            other_container.Begin(),
            this_size_bytes
        ) == 0;
    }

    template <class TaskSystem, class Lambda>
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda)
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform parallel for-each");

        task_system.ParallelForEach(
            *static_cast<Container *>(this),
            std::forward<Lambda>(lambda)
        );
    }

    template <class TaskSystem, class Lambda>
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda) const
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform parallel for-each");

        task_system.ParallelForEach(
            *static_cast<const Container *>(this),
            std::forward<Lambda>(lambda)
        );
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (auto it = static_cast<const Container *>(this)->Begin(); it != static_cast<const Container *>(this)->End(); ++it) {
            hc.Add(*it);
        }

        return hc;
    }

};

} // namespace hyperion

#endif
