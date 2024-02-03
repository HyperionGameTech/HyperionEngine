#ifndef HYPERION_V2_DOTNET_CLASS_OBJECT_HPP
#define HYPERION_V2_DOTNET_CLASS_OBJECT_HPP

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
class ClassObject;
class ClassObjectHolder;
} // namespace hyperion::dotnet

extern "C" {

    struct ManagedClass
    {
        hyperion::Int32                 type_hash;
        hyperion::dotnet::ClassObject   *class_object;
    };
}

namespace hyperion::dotnet {

class ClassObject
{
private:
    String                          m_name;
    HashMap<String, ManagedMethod>  m_methods;

public:
    // Function to create a new object of this class with the given arguments
    using NewObjectFunction     = ManagedObject(*)(void);
    using FreeObjectFunction    = void(*)(ManagedObject);

    ClassObject(ClassObjectHolder *parent, String name)
        : m_parent(parent),
          m_name(std::move(name)),
          m_new_object_fptr(nullptr)
    {
    }

    ClassObject(const ClassObject &)                = delete;
    ClassObject &operator=(const ClassObject &)     = delete;
    ClassObject(ClassObject &&) noexcept            = delete;
    ClassObject &operator=(ClassObject &&) noexcept = delete;
    ~ClassObject()                                  = default;

    const String &GetName() const
        { return m_name; }

    NewObjectFunction GetNewObjectFunction() const
        { return m_new_object_fptr; }

    void SetNewObjectFunction(NewObjectFunction new_object_fptr)
        { m_new_object_fptr = new_object_fptr; }

    FreeObjectFunction GetFreeObjectFunction() const
        { return m_free_object_fptr; }

    void SetFreeObjectFunction(FreeObjectFunction free_object_fptr)
        { m_free_object_fptr = free_object_fptr; }

    bool HasMethod(const String &method_name) const
        { return m_methods.Find(method_name) != m_methods.End(); }

    void AddMethod(const String &method_name, ManagedMethod &&method_object)
        { m_methods[method_name] = std::move(method_object); }

    ManagedObject NewObject();
    void FreeObject(ManagedObject object);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const String &method_name, ManagedObject instance, Args &&... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        auto it = m_methods.Find(method_name);
        AssertThrowMsg(it != m_methods.End(), "Method not found");

        ManagedMethod &method_object = it->second;

        void *args_vptr[] = { &args... };

        if constexpr (std::is_void_v<ReturnType>) {
            InvokeMethod(&method_object, instance.ptr, args_vptr, nullptr);
        } else {
            ValueStorage<ReturnType> return_value_storage;
            void *result_vptr = InvokeMethod(&method_object, instance.ptr, args_vptr, return_value_storage.GetPointer());

            return return_value_storage.Get();
        }
    }

    template <class ReturnType, class... Args>
    ReturnType InvokeStaticMethod(const String &method_name, Args &&... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        auto it = m_methods.Find(method_name);
        AssertThrowMsg(it != m_methods.End(), "Method not found");

        ManagedMethod &method_object = it->second;

        void *args_vptr[] = { &args... };

        if constexpr (std::is_void_v<ReturnType>) {
            InvokeMethod(&method_object, nullptr, args_vptr, nullptr);
        } else {
            ValueStorage<ReturnType> return_value_storage;
            void *result_vptr = InvokeMethod(&method_object, nullptr, args_vptr, return_value_storage.GetPointer());

            return return_value_storage.Get();
        }
    }

private:
    void *InvokeMethod(ManagedMethod *method_object, void *this_vptr, void **args_vptr, void *return_value_vptr);

    ClassObjectHolder   *m_parent;
    NewObjectFunction   m_new_object_fptr;
    FreeObjectFunction  m_free_object_fptr;
};

} // namespace hyperion::dotnet

#endif