#ifndef HYPERION_DOTNET_HELPERS_HPP
#define HYPERION_DOTNET_HELPERS_HPP

#include <core/Defines.hpp>

#include <core/containers/String.hpp>

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
template <class T> struct TransformArgument<T, std::enable_if_t< std::is_class_v<T> && IsPODType<T> > > : DefaultTransformArgument<T> { };

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

template <>
struct TransformArgument<String>
{
    HYP_API const char *operator()(const String &value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<UTF8StringView>
{
    HYP_API const char *operator()(UTF8StringView value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<ANSIString>
{
    HYP_API const char *operator()(const ANSIString &value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<ANSIStringView>
{
    HYP_API const char *operator()(ANSIStringView value) const
    {
        return value.Data();
    }
};

} // namespace detail

} // namespace hyperion::dotnet

#endif