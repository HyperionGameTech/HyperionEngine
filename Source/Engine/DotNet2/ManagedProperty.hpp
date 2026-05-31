/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/StringView.hpp>

#include <DotNET/Helpers.hpp>
#include <DotNET/ManagedAttribute.hpp>

#include <DotNET/interop/ManagedGuid.hpp>

#include <Core/Types.hpp>

namespace Hyperion::dotnet {

class ManagedObject;

class ManagedProperty
{
public:
    ManagedProperty() = default;

    ManagedProperty(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    ManagedProperty(ManagedGuid guid, ManagedAttributeSet&& attributes)
        : m_guid(guid),
          m_attributes(std::move(attributes))
    {
    }

    ManagedProperty(const ManagedProperty& other) = delete;
    ManagedProperty& operator=(const ManagedProperty& other) = delete;

    ManagedProperty(ManagedProperty&& other) noexcept = default;
    ManagedProperty& operator=(ManagedProperty&& other) noexcept = default;

    ~ManagedProperty() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE const ManagedAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    template <class ReturnType>
    ReturnType InvokeGetter(const ManagedObject* pManagedObject)
    {
        BoxedValue returnValue;
        InvokeGetter_Internal(pManagedObject, &returnValue);

        return std::move(returnValue.Get<ReturnType>());
    }

    template <class T>
    void InvokeSetter(const ManagedObject* pManagedObject, T&& value)
    {
        BoxedValue returnValue(std::forward<T>(value));
        const BoxedValue* returnValuePtr = &returnValue;

        return InvokeSetter_Internal(pManagedObject, &returnValuePtr);
    }

private:
    void InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* outBoxed);
    void InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** boxedValue);

    ManagedGuid m_guid;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
