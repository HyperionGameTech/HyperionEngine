/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_DOTNET_CLASS_HPP
#define HYPERION_V2_DOTNET_CLASS_HPP

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
class Object;
class Class;
class ClassHolder;
} // namespace hyperion::dotnet

extern "C" {

struct ManagedClass
{
    hyperion::int32         type_hash;
    hyperion::dotnet::Class *class_object;
};

}

namespace hyperion::dotnet {

class Class
{
public:
    // Function to create a new object of this class with the given arguments
    using NewObjectFunction     = ManagedObject(*)(void);
    using FreeObjectFunction    = void(*)(ManagedObject);

    Class(ClassHolder *parent, String name)
        : m_parent(parent),
          m_name(std::move(name)),
          m_new_object_fptr(nullptr)
    {
    }

    Class(const Class &)                = delete;
    Class &operator=(const Class &)     = delete;
    Class(Class &&) noexcept            = delete;
    Class &operator=(Class &&) noexcept = delete;
    ~Class()                            = default;

    const String &GetName() const
        { return m_name; }

    ClassHolder *GetParent() const
        { return m_parent; }

    NewObjectFunction GetNewObjectFunction() const
        { return m_new_object_fptr; }

    void SetNewObjectFunction(NewObjectFunction new_object_fptr)
        { m_new_object_fptr = new_object_fptr; }

    FreeObjectFunction GetFreeObjectFunction() const
        { return m_free_object_fptr; }

    void SetFreeObjectFunction(FreeObjectFunction free_object_fptr)
        { m_free_object_fptr = free_object_fptr; }

    /*! \brief Check if a method exists by name.
     *
     *  \param method_name The name of the method to check.
     *
     *  \return True if the method exists, otherwise false.
     */
    bool HasMethod(const String &method_name) const
        { return m_methods.Find(method_name) != m_methods.End(); }

    /*! \brief Get a method by name.
     *
     *  \param method_name The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    ManagedMethod *GetMethod(const String &method_name)
    {
        auto it = m_methods.Find(method_name);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by name.
     *
     *  \param method_name The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    const ManagedMethod *GetMethod(const String &method_name) const
    {
        auto it = m_methods.Find(method_name);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Add a method to this class.
     *
     *  \param method_name The name of the method to add.
     *  \param method_object The method object to add.
     */
    void AddMethod(const String &method_name, ManagedMethod &&method_object)
        { m_methods[method_name] = std::move(method_object); }

    /*! \brief Get all methods of this class.
     *
     *  \return A reference to the map of methods.
     */
    const HashMap<String, ManagedMethod> &GetMethods() const
        { return m_methods; }

    /*! \brief Create a new managed object of this class.
     *     The new object will be managed by the .NET runtime and will be freed when the unique pointer goes out of scope.
     *      The returned object will hold a reference to this class instance, so it will need to remain valid for the lifetime of the object.
     *
     *  \return A unique pointer to the new managed object.
     */
    UniquePtr<Object> NewObject();

    template <class ReturnType, class... Args>
    ReturnType InvokeStaticMethod(const String &method_name, Args &&... args)
    {
        static_assert(std::is_void_v<ReturnType> || std::is_trivial_v<ReturnType>, "Return type must be trivial to be used in interop");
        static_assert(std::is_void_v<ReturnType> || std::is_object_v<ReturnType>, "Return type must be either a value type or a pointer type to be used in interop (no references)");

        auto it = m_methods.Find(method_name);
        AssertThrowMsg(it != m_methods.End(), "Method not found");

        const ManagedMethod &method_object = it->second;

        void *args_vptr[] = { &args... };

        if constexpr (std::is_void_v<ReturnType>) {
            InvokeStaticMethod(&method_object, args_vptr, nullptr);
        } else {
            ValueStorage<ReturnType> return_value_storage;
            void *result_vptr = InvokeStaticMethod(&method_object, args_vptr, return_value_storage.GetPointer());

            return return_value_storage.Get();
        }
    }

private:
    void *InvokeStaticMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr);

    String                          m_name;
    HashMap<String, ManagedMethod>  m_methods;

    ClassHolder                     *m_parent;

    NewObjectFunction               m_new_object_fptr;
    FreeObjectFunction              m_free_object_fptr;
};

} // namespace hyperion::dotnet

#endif