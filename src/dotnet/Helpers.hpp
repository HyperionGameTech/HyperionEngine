#ifndef HYPERION_DOTNET_HELPERS_HPP
#define HYPERION_DOTNET_HELPERS_HPP

#include <core/Defines.hpp>

#include <core/ID.hpp>

#include <type_traits>

namespace hyperion::dotnet {

class Object;

namespace detail {

template <class T, class T2 = void>
struct TransformArgument;

template <class T>
struct DefaultTransformArgument
{
    HYP_FORCE_INLINE T operator()(T value) const
    {
        return value;
    }
};

template <class T> struct TransformArgument<T, std::enable_if_t< std::is_arithmetic_v<T> > > : DefaultTransformArgument<T> { };
template <class T> struct TransformArgument<T, std::enable_if_t< std::is_class_v<T> && std::is_standard_layout_v<T> && std::is_trivial_v<T> > > : DefaultTransformArgument<T> { };

template <> struct TransformArgument<void *> : DefaultTransformArgument<void *> { };
template <> struct TransformArgument<char *> : DefaultTransformArgument<char *> { };
template <> struct TransformArgument<const char *> : DefaultTransformArgument<const char *> { };

template <class T>
struct TransformArgument<ID<T>>
{
    HYP_FORCE_INLINE typename ID<T>::ValueType operator()(ID<T> id) const
    {
        return id.Value();
    }
};

template <>
struct TransformArgument<Object *>
{
    HYP_API void *operator()(Object *value) const;
};

} // namespace detail

} // namespace hyperion::dotnet

#endif