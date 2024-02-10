#ifndef HYPERION_V2_DOTNET_OBJECT_HPP
#define HYPERION_V2_DOTNET_OBJECT_HPP

#include <Engine.hpp>
#include <Types.hpp>

#include <core/lib/Mutex.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/String.hpp>

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

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const String &method_name, Args &&... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        const ManagedMethod *method_ptr = GetMethod(method_name);
        AssertThrowMsg(method_ptr != nullptr, "Method %s not found", method_name.Data());

        if constexpr (sizeof...(args) != 0) {
            void *args_vptr[] = { &args... };

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod(method_ptr, args_vptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod(method_ptr, args_vptr, return_value_storage.GetPointer());

                return return_value_storage.Get();
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod(method_ptr, nullptr, nullptr);
            } else {
                ValueStorage<ReturnType> return_value_storage;
                void *result_vptr = InvokeMethod(method_ptr, nullptr, return_value_storage.GetPointer());

                return return_value_storage.Get();
            }
        }
    }

private:
    const ManagedMethod *GetMethod(const String &method_name) const;
    void *InvokeMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr);

    Class          *m_class_ptr;
    ManagedObject   m_managed_object;
};

} // namespace hyperion::dotnet

#endif