/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/containers/HashMap.hpp>

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

class ManagedClass;
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

    RC<ManagedClass> NewClass(const HypClass* hypClass, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, ManagedClass* parentClass, uint32 flags);
    RC<ManagedClass> FindClassByName(const char* typeName);
    RC<ManagedClass> FindClassByTypeHash(int32 typeHash);

    HYP_FORCE_INLINE InvokeGetterFunction GetInvokeGetterFunction() const
    {
        return m_invokeGetterFptr;
    }

    HYP_FORCE_INLINE void SetInvokeGetterFunction(InvokeGetterFunction invokeGetterFptr)
    {
        m_invokeGetterFptr = invokeGetterFptr;
    }

    HYP_FORCE_INLINE InvokeSetterFunction GetInvokeSetterFunction() const
    {
        return m_invokeSetterFptr;
    }

    HYP_FORCE_INLINE void SetInvokeSetterFunction(InvokeSetterFunction invokeSetterFptr)
    {
        m_invokeSetterFptr = invokeSetterFptr;
    }

    HYP_FORCE_INLINE bool IsLoaded() const
    {
        return m_guid.IsValid();
    }

    bool Unload();

private:
    EnumFlags<AssemblyFlags> m_flags;

    ManagedGuid m_guid;

    HashMap<int32, RC<ManagedClass>> m_classObjects;

    // Function pointer to invoke a managed method
    InvokeGetterFunction m_invokeGetterFptr;
    InvokeSetterFunction m_invokeSetterFptr;
};

} // namespace dotnet
} // namespace hyperion
