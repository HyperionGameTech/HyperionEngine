/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_OBJECT_HPP
#define HYPERION_DOTNET_OBJECT_HPP

#include <Engine.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/StringView.hpp>

#include <dotnet/interop/ManagedMethod.hpp>
#include <dotnet/interop/ManagedObject.hpp>

namespace hyperion::dotnet {

class Class;

class Object
{
public:
    Object(Class *class_ptr, ManagedObject managed_object);

    Object(const Object &)                  = delete;
    Object &operator=(const Object &)       = delete;
    Object(Object &&) noexcept              = delete;
    Object &operator=(Object &&) noexcept   = delete;

    // Destructor frees the managed object
    ~Object();

    Class *GetClass() const
        { return m_class_ptr; }

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const ManagedMethod *method_ptr, Args &&... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        if constexpr (sizeof...(args) != 0) {
            void *args_vptr[] = { &args... };

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod(method_ptr, args_vptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod(method_ptr, args_vptr, return_value_storage.GetPointer());

                return std::move(return_value_storage.Get());
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod(method_ptr, nullptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod(method_ptr, nullptr, return_value_storage.GetPointer());

                return std::move(return_value_storage.Get());
            }
        }
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE
    ReturnType InvokeMethodByName(UTF8StringView method_name, Args &&... args)
    {
        const ManagedMethod *method_ptr = GetMethod(method_name);
        AssertThrowMsg(method_ptr != nullptr, "Method %s not found", method_name.Data());

        return InvokeMethod<ReturnType>(method_ptr, std::forward<Args>(args)...);
    }

private:
    const ManagedMethod *GetMethod(UTF8StringView method_name) const;
    void *InvokeMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr);

    Class          *m_class_ptr;
    ManagedObject   m_managed_object;
};

} // namespace hyperion::dotnet

#endif