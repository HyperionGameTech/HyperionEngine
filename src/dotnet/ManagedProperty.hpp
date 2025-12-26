/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/Helpers.hpp>
#include <dotnet/ManagedAttribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <core/Types.hpp>

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
        BoxedValue returnHypData;
        InvokeGetter_Internal(pManagedObject, &returnHypData);

        return std::move(returnHypData.Get<ReturnType>());
    }

    template <class T>
    void InvokeSetter(const ManagedObject* pManagedObject, T&& value)
    {
        BoxedValue valueHypData(std::forward<T>(value));
        const BoxedValue* valueHypDataPtr = &valueHypData;

        return InvokeSetter_Internal(pManagedObject, &valueHypDataPtr);
    }

private:
    void InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* outReturnHypData);
    void InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** valueHypData);

    ManagedGuid m_guid;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
