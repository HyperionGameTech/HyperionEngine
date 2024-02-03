#ifndef HYP_DOTNET_ASSEMBLY_HPP
#define HYP_DOTNET_ASSEMBLY_HPP

#include <core/lib/Mutex.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <dotnet/Types.hpp>

namespace hyperion {
namespace dotnet {

extern "C" {
    struct ManagedMethod;
}


class ClassObject;

class ClassObjectHolder
{
public:
    using InvokeMethodFunction = void *(*)(ManagedMethod *, void *, void **, void *);

    ClassObjectHolder();
    ClassObjectHolder(const ClassObjectHolder &)                    = delete;
    ClassObjectHolder &operator=(const ClassObjectHolder &)         = delete;
    ClassObjectHolder(ClassObjectHolder &&) noexcept                = delete;
    ClassObjectHolder &operator=(ClassObjectHolder &&) noexcept     = delete;
    ~ClassObjectHolder()                                            = default;

    ClassObject *GetOrCreateClassObject(Int32 type_hash, const char *type_name);
    ClassObject *FindClassByName(const char *type_name);

    InvokeMethodFunction GetInvokeMethodFunction() const
        { return m_invoke_method_fptr; }

    void SetInvokeMethodFunction(InvokeMethodFunction invoke_method_fptr)
        { m_invoke_method_fptr = invoke_method_fptr; }

private:
    HashMap<Int32, UniquePtr<ClassObject>>  m_class_objects;

    // Function pointer to invoke a managed method
    InvokeMethodFunction                    m_invoke_method_fptr;
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

    ClassObjectHolder &GetClassObjectHolder()
        { return m_class_object_holder; }

    const ClassObjectHolder &GetClassObjectHolder() const
        { return m_class_object_holder; }

private:
    ClassObjectHolder   m_class_object_holder;
};

} // namespace dotnet
} // namespace hyperion

#endif