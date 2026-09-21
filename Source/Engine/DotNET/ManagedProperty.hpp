/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/StringView.hpp>

#include <DotNET/Helpers.hpp>
#include <DotNET/ManagedAttribute.hpp>

#include <DotNET/Interop/ManagedGuid.hpp>

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

        if (!InvokeGetter_Internal(pManagedObject, &returnValue) || returnValue.IsNull())
        {
            return ReturnType();
        }

        return std::move(returnValue.Get<ReturnType>());
    }

    template <class T>
    bool InvokeSetter(const ManagedObject* pManagedObject, T&& value)
    {
        BoxedValue returnValue(std::forward<T>(value));
        const BoxedValue* returnValuePtr = &returnValue;

        return InvokeSetter_Internal(pManagedObject, &returnValuePtr);
    }

private:
    bool InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* outBoxed);
    bool InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** boxedValue);

    ManagedGuid m_guid;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
