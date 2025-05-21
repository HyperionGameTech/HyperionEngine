/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SERIALIZATION_WRAPPER_HPP
#define HYPERION_SERIALIZATION_WRAPPER_HPP

#include <core/Defines.hpp>

#include <core/Handle.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class Node;

template <class T>
struct SerializationWrapper
{
    using Type = std::conditional_t<
        has_handle_definition<T> && !std::is_same_v<T, Node>,
        Handle<T>,
        T
    >;

    template <class Ty> // use template param to disambiguate const v.s non-const ref
    static auto &&Unwrap(Ty &&value)
    {
        if constexpr (has_handle_definition<T>) {
            AssertThrow(value.IsValid());

            return *value;
        } else {
            return value;
        }
    }

    static void OnPostLoad(const Type &value) { }
};

template <class T>
struct SerializationWrapperReverseMapping { using Type = T; };

template <class T>
struct SerializationWrapperReverseMapping<Handle<T>> { using Type = T; };

template <class T>
struct SerializationWrapper<RC<T>>
{
    using Type = RC<T>;

    static const T &Unwrap(const Type &value)
    {
        AssertThrow(value != nullptr);

        return *value;
    }

    static void OnPostLoad(const Type &value) { }
};

template <class T>
struct SerializationWrapperReverseMapping<RC<T>> { using Type = T; };

template <>
struct SerializationWrapper<Node>
{
    using Type = Handle<Node>;

    HYP_API static Node &Unwrap(const Type &value);

    HYP_API static void OnPostLoad(const Type &value);
};

template <>
struct SerializationWrapperReverseMapping<Handle<Node>> { using Type = Node; };

} // namespace hyperion

#endif