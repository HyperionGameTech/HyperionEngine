/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <DotNET/ManagedAttribute.hpp>
#include <DotNET/Helpers.hpp>

#include <DotNET/Types.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion::dotnet {

struct ObjectReference;

class ManagedMethod
{
public:
    ManagedMethod()
        : m_invokeFptr(nullptr)
    {
    }

    ManagedMethod(ManagedGuid guid, InvokeMethodFunction invokeFptr)
        : m_guid(guid),
          m_invokeFptr(invokeFptr)
    {
    }

    ManagedMethod(ManagedGuid guid, InvokeMethodFunction invokeFptr, ManagedAttributeSet&& attributes)
        : m_guid(guid),
          m_invokeFptr(invokeFptr),
          m_attributes(std::move(attributes))
    {
    }

    ManagedMethod(const ManagedMethod& other) = delete;
    ManagedMethod& operator=(const ManagedMethod& other) = delete;

    ManagedMethod(ManagedMethod&& other) noexcept = default;
    ManagedMethod& operator=(ManagedMethod&& other) noexcept = default;

    ~ManagedMethod() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE InvokeMethodFunction GetFunctionPointer() const
    {
        return m_invokeFptr;
    }

    HYP_FORCE_INLINE const ManagedAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE void Invoke(ObjectReference* thisObjectReference, const BoxedValue** argsBoxed, BoxedValue* outBoxed) const
    {
        m_invokeFptr(thisObjectReference, argsBoxed, outBoxed);
    }

private:
    ManagedGuid m_guid;
    InvokeMethodFunction m_invokeFptr;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
