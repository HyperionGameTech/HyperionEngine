/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_DOTNET_ASSEMBLY_HPP
#define HYP_DOTNET_ASSEMBLY_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

namespace hyperion {
namespace dotnet {

extern "C" {
    struct ManagedMethod;
}

class Class;
class Assembly;

class ClassHolder
{
public:
    friend class Assembly;

    using InvokeMethodFunction = void *(*)(ManagedGuid, ManagedGuid, void **, void *);

    ClassHolder(Weak<Assembly> &&owner_assembly);
    ClassHolder(const ClassHolder &)                = delete;
    ClassHolder &operator=(const ClassHolder &)     = delete;
    ClassHolder(ClassHolder &&) noexcept            = delete;
    ClassHolder &operator=(ClassHolder &&) noexcept = delete;
    ~ClassHolder()                                  = default;

    bool CheckAssemblyLoaded() const;

    const Weak<Assembly> &GetOwnerAssembly() const
        { return m_owner_assembly; }

    Class *GetOrCreateClassObject(int32 type_hash, const char *type_name);
    Class *FindClassByName(const char *type_name);

    InvokeMethodFunction GetInvokeMethodFunction() const
        { return m_invoke_method_fptr; }

    void SetInvokeMethodFunction(InvokeMethodFunction invoke_method_fptr)
        { m_invoke_method_fptr = invoke_method_fptr; }

private:
    Weak<Assembly>                      m_owner_assembly;

    HashMap<int32, UniquePtr<Class>>    m_class_objects;

    // Function pointer to invoke a managed method
    InvokeMethodFunction                m_invoke_method_fptr;
};

class Assembly : public EnableRefCountedPtrFromThis<Assembly>
{
public:
    Assembly();
    Assembly(const Assembly &)                  = delete;
    Assembly &operator=(const Assembly &)       = delete;
    Assembly(Assembly &&) noexcept              = delete;
    Assembly &operator=(Assembly &&) noexcept   = delete;
    ~Assembly();

    ManagedGuid &GetGuid()
        { return m_guid; }

    const ManagedGuid &GetGuid() const
        { return m_guid; }

    ClassHolder &GetClassObjectHolder()
        { return m_class_object_holder; }

    const ClassHolder &GetClassObjectHolder() const
        { return m_class_object_holder; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsLoaded() const
        { return m_guid.IsValid(); }

    bool Unload();

private:
    ManagedGuid     m_guid;
    ClassHolder     m_class_object_holder;
};

} // namespace dotnet
} // namespace hyperion

#endif