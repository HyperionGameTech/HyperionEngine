/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYP_DOTNET_ASSEMBLY_HPP
#define HYP_DOTNET_ASSEMBLY_HPP

#include <core/lib/Mutex.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <dotnet/Types.hpp>
#include <dotnet/interop/ManagedGuid.hpp>

namespace hyperion {
namespace dotnet {

extern "C" {
    struct ManagedMethod;
}

class Class;

class ClassHolder
{
public:
    using InvokeMethodFunction = void *(*)(ManagedGuid, ManagedGuid, void **, void *);

    ClassHolder();
    ClassHolder(const ClassHolder &)                = delete;
    ClassHolder &operator=(const ClassHolder &)     = delete;
    ClassHolder(ClassHolder &&) noexcept            = delete;
    ClassHolder &operator=(ClassHolder &&) noexcept = delete;
    ~ClassHolder()                                  = default;

    Class *GetOrCreateClassObject(int32 type_hash, const char *type_name);
    Class *FindClassByName(const char *type_name);

    InvokeMethodFunction GetInvokeMethodFunction() const
        { return m_invoke_method_fptr; }

    void SetInvokeMethodFunction(InvokeMethodFunction invoke_method_fptr)
        { m_invoke_method_fptr = invoke_method_fptr; }

private:
    HashMap<int32, UniquePtr<Class>>    m_class_objects;

    // Function pointer to invoke a managed method
    InvokeMethodFunction                m_invoke_method_fptr;
};

class Assembly
{
public:
    Assembly();
    Assembly(const Assembly &)                  = delete;
    Assembly &operator=(const Assembly &)       = delete;
    Assembly(Assembly &&) noexcept              = delete;
    Assembly &operator=(Assembly &&) noexcept   = delete;
    ~Assembly();

    ClassHolder &GetClassObjectHolder()
        { return m_class_object_holder; }

    const ClassHolder &GetClassObjectHolder() const
        { return m_class_object_holder; }

private:
    ClassHolder   m_class_object_holder;
};

} // namespace dotnet
} // namespace hyperion

#endif