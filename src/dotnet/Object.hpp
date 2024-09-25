/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_OBJECT_HPP
#define HYPERION_DOTNET_OBJECT_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <core/ID.hpp>

#include <dotnet/interop/ManagedMethod.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <type_traits>

namespace hyperion::dotnet {

class Class;
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

class Object
{
public:
    Object(Class *class_ptr, ObjectReference object_reference);

    Object(const Object &)                  = delete;
    Object &operator=(const Object &)       = delete;
    Object(Object &&) noexcept              = delete;
    Object &operator=(Object &&) noexcept   = delete;

    // Destructor frees the managed object
    ~Object();

    HYP_FORCE_INLINE Class *GetClass() const
        { return m_class_ptr; }

    HYP_FORCE_INLINE const ObjectReference &GetObjectReference() const
        { return m_object_reference; }

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const ManagedMethod *method_ptr, Args... args)
    {
        return InvokeMethod_Internal<ReturnType>(method_ptr, detail::TransformArgument<Args>{}(args)...);
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE ReturnType InvokeMethodByName(UTF8StringView method_name, Args... args)
    {
        const ManagedMethod *method_ptr = GetMethod(method_name);
        AssertThrowMsg(method_ptr != nullptr, "Method %s not found", method_name.Data());

        return InvokeMethod_Internal<ReturnType>(method_ptr, detail::TransformArgument<Args>{}(args)...);
    }

private:
    const ManagedMethod *GetMethod(UTF8StringView method_name) const;

    void *InvokeMethod_Internal(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod_Internal(const ManagedMethod *method_ptr, Args... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        if constexpr (sizeof...(args) != 0) {
            void *args_vptr[] = { &args... };

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, args_vptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod_Internal(method_ptr, args_vptr, return_value_storage.GetPointer());

                return std::move(return_value_storage.Get());
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, nullptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod_Internal(method_ptr, nullptr, return_value_storage.GetPointer());

                return std::move(return_value_storage.Get());
            }
        }
    }

    Class           *m_class_ptr;
    ObjectReference m_object_reference;
};

} // namespace hyperion::dotnet

#endif