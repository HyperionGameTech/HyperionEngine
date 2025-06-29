/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_DOTNET_ASSEMBLY_HPP
#define HYP_DOTNET_ASSEMBLY_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <dotnet/Types.hpp>

namespace hyperion {

class HypClass;
struct HypData;

enum class AssemblyFlags : uint32
{
    NONE = 0x0,
    CORE_ASSEMBLY = 0x1
};

HYP_MAKE_ENUM_FLAGS(AssemblyFlags)

namespace dotnet {

class Class;
class Assembly;
class Method;

class HYP_API Assembly : public EnableRefCountedPtrFromThis<Assembly>
{
public:
    Assembly();
    Assembly(EnumFlags<AssemblyFlags> flags);
    Assembly(const Assembly&) = delete;
    Assembly& operator=(const Assembly&) = delete;
    Assembly(Assembly&&) noexcept = delete;
    Assembly& operator=(Assembly&&) noexcept = delete;
    ~Assembly();

    HYP_FORCE_INLINE ManagedGuid& GetGuid()
    {
        return m_guid;
    }

    HYP_FORCE_INLINE const ManagedGuid& GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE EnumFlags<AssemblyFlags> GetFlags() const
    {
        return m_flags;
    }

    RC<Class> NewClass(const HypClass* hyp_class, int32 type_hash, const char* type_name, uint32 type_size, TypeId type_id, Class* parent_class, uint32 flags);
    RC<Class> FindClassByName(const char* type_name);
    RC<Class> FindClassByTypeHash(int32 type_hash);

    HYP_FORCE_INLINE InvokeGetterFunction GetInvokeGetterFunction() const
    {
        return m_invoke_getter_fptr;
    }

    HYP_FORCE_INLINE void SetInvokeGetterFunction(InvokeGetterFunction invoke_getter_fptr)
    {
        m_invoke_getter_fptr = invoke_getter_fptr;
    }

    HYP_FORCE_INLINE InvokeSetterFunction GetInvokeSetterFunction() const
    {
        return m_invoke_setter_fptr;
    }

    HYP_FORCE_INLINE void SetInvokeSetterFunction(InvokeSetterFunction invoke_setter_fptr)
    {
        m_invoke_setter_fptr = invoke_setter_fptr;
    }

    HYP_FORCE_INLINE bool IsLoaded() const
    {
        return m_guid.IsValid();
    }

    bool Unload();

private:
    EnumFlags<AssemblyFlags> m_flags;

    ManagedGuid m_guid;

    HashMap<int32, RC<Class>> m_class_objects;

    // Function pointer to invoke a managed method
    InvokeGetterFunction m_invoke_getter_fptr;
    InvokeSetterFunction m_invoke_setter_fptr;
};

} // namespace dotnet
} // namespace hyperion

#endif