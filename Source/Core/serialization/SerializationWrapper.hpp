/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/memory/RefCountedPtr.hpp>

namespace Hyperion {

class Node;

template <class T>
struct SerializationWrapper
{
    using Type = std::conditional_t<std::is_base_of_v<ObjectBase, T>, Handle<T>, T>;
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

} // namespace Hyperion
