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
    using Type = std::conditional_t<std::is_base_of_v<HypObjectBase, T>, Handle<T>, T>;
};

template <class T>
struct SerializationWrapperReverseMapping
{
    using Type = T;
};

template <class T>
struct SerializationWrapperReverseMapping<Handle<T>>
{
    using Type = T;
};

template <class T>
struct SerializationWrapper<RC<T>>
{
    using Type = RC<T>;
};

template <class T>
struct SerializationWrapperReverseMapping<RC<T>>
{
    using Type = T;
};

template <>
struct SerializationWrapper<Node>
{
    using Type = Handle<Node>;
};

template <>
struct SerializationWrapperReverseMapping<Handle<Node>>
{
    using Type = Node;
};

} // namespace hyperion

#endif