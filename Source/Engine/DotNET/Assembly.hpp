/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/Map.hpp>

#include <DotNET/Types.hpp>

namespace Hyperion {

class Class;
struct BoxedValue;

enum class AssemblyFlags : uint32
{
    NONE = 0x0,
    CORE_ASSEMBLY = 0x1
};

HYP_MAKE_ENUM_FLAGS(AssemblyFlags)

namespace dotnet {

class ManagedClass;
class Assembly;
class ManagedMethod;

class ENGINE_API Assembly : public SharedFromThis<Assembly>
{
public:
    explicit Assembly(const ManagedGuid& guid);
    explicit Assembly(const ManagedGuid& guid, EnumFlags<AssemblyFlags> flags);

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

    SharedPtr<ManagedClass> NewClass(const Class* cls, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, ManagedClass* parentClass, uint32 flags);
    SharedPtr<ManagedClass> FindClassByName(const char* typeName);
    SharedPtr<ManagedClass> FindClassByTypeHash(int32 typeHash);

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

    TMap<int32, SharedPtr<ManagedClass>> m_classObjects;

    // Function pointer to invoke a managed method
    InvokeGetterFunction m_invokeGetterFptr;
    InvokeSetterFunction m_invokeSetterFptr;
};

} // namespace dotnet
} // namespace Hyperion
