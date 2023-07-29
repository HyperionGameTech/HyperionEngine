#ifndef HYPERION_V2_LIB_CONTAINER_BASE_H
#define HYPERION_V2_LIB_CONTAINER_BASE_H

#include "Pair.hpp"
#include <util/Defines.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Container, class Key>
class ContainerBase
{
protected:
    using Base = ContainerBase;
public:
    using KeyType = Key;

    ContainerBase() { }
    ~ContainerBase() { }

    auto &Get(KeyType index)
    {
        AssertThrow(index < static_cast<KeyType>(static_cast<const Container *>(this)->Size()));
        return static_cast<Container *>(this)->operator[](index);
    }

    [[nodiscard]] const auto &Get(KeyType index) const
        { return const_cast<const ContainerBase *>(this)->Get(index); }

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
    [[nodiscard]] auto Find(const T &value)
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
    [[nodiscard]] auto Find(const T &value) const
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

    template <class Function>
    [[nodiscard]] auto FindIf(Function &&pred)
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
    [[nodiscard]] auto FindIf(Function &&pred) const
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
    [[nodiscard]] auto LowerBound(const T &key)
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
    [[nodiscard]] auto LowerBound(const T &key) const
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
    [[nodiscard]] bool Contains(const T &value) const
    {
        return static_cast<const Container *>(this)->Find(value)
            != static_cast<const Container *>(this)->End();
    }

    template <class Lambda>
    [[nodiscard]] bool Any(Lambda &&lambda) const
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
    [[nodiscard]] bool Every(Lambda &&lambda) const
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

    [[nodiscard]] auto Sum() const
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

    [[nodiscard]] auto Avg() const
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
    [[nodiscard]] KeyType IndexOf(ConstIterator iter) const
    {
        static_assert(std::is_convertible_v<decltype(iter),
            typename Container::ConstIterator>, "Iterator type does not match container");

        return static_cast<KeyType>(std::distance(static_cast<const Container *>(this)->Begin(), iter));
    }

    template <class TaskSystem, class Lambda>
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda)
    {
        task_system.ParallelForEach(
            *static_cast<Container *>(this),
            std::forward<Lambda>(lambda)
        );
    }
    
    [[nodiscard]] HashCode GetHashCode() const
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
