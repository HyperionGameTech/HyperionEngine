#ifndef HYPERION_V2_DOTNET_SUPPORT_CLASS_OBJECT_HPP
#define HYPERION_V2_DOTNET_SUPPORT_CLASS_OBJECT_HPP

#include <Engine.hpp>
#include <Types.hpp>

#include <core/lib/Mutex.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/String.hpp>

namespace hyperion::dotnet {
class ClassObject;
class ClassObjectHolder;
} // namespace hyperion::dotnet

extern "C" {
    struct ManagedMethod
    {
        void    *method_info_ptr;
    };

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
    ClassObject(ClassObjectHolder *parent, String name)
        : m_parent(parent),
          m_name(std::move(name))
    {
    }

    ClassObject(const ClassObject &)                = delete;
    ClassObject &operator=(const ClassObject &)     = delete;
    ClassObject(ClassObject &&) noexcept            = delete;
    ClassObject &operator=(ClassObject &&) noexcept = delete;
    ~ClassObject()                                  = default;

    const String &GetName() const
        { return m_name; }

    bool HasMethod(const String &method_name) const
        { return m_methods.Find(method_name) != m_methods.End(); }

    void AddMethod(const String &method_name, ManagedMethod &&method_object)
        { m_methods[method_name] = std::move(method_object); }

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const String &method_name, Args &&... args)
    {
        auto it = m_methods.Find(method_name);
        AssertThrowMsg(it != m_methods.End(), "Method not found");

        ManagedMethod &method_object = it->second;

        void **args_vptr = new void *[sizeof...(Args)] { reinterpret_cast<void *>(&args)... };
        void *result_vptr = InvokeMethod(&method_object, args_vptr);
        delete[] args_vptr;

        return reinterpret_cast<ReturnType>(result_vptr);
    }

private:
    void *InvokeMethod(ManagedMethod *method_object, void **args_vptr);

    template <class T>
    static void *MakeVoidPointer(T &&value)
    {
        if constexpr (std::is_pointer_v<T>) {
            return reinterpret_cast<void *>(value);
        }

        return reinterpret_cast<void *>(&value);
    }

    ClassObjectHolder *m_parent;
};

} // namespace hyperion::dotnet

#endif