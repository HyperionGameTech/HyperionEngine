/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/Helpers.hpp>
#include <dotnet/ManagedAttribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <core/Types.hpp>

namespace hyperion::dotnet {

class ManagedObject;

class Property
{
public:
    Property() = default;

    Property(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    Property(ManagedGuid guid, ManagedAttributeSet&& attributes)
        : m_guid(guid),
          m_attributes(std::move(attributes))
    {
    }

    Property(const Property& other) = delete;
    Property& operator=(const Property& other) = delete;

    Property(Property&& other) noexcept = default;
    Property& operator=(Property&& other) noexcept = default;

    ~Property() = default;

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
        HypData returnHypData;
        InvokeGetter_Internal(pManagedObject, &returnHypData);

        return std::move(returnHypData.Get<ReturnType>());
    }

    template <class T>
    void InvokeSetter(const ManagedObject* pManagedObject, T&& value)
    {
        HypData valueHypData(std::forward<T>(value));
        const HypData* valueHypDataPtr = &valueHypData;

        return InvokeSetter_Internal(pManagedObject, &valueHypDataPtr);
    }

private:
    void InvokeGetter_Internal(const ManagedObject* pManagedObject, HypData* outReturnHypData);
    void InvokeSetter_Internal(const ManagedObject* pManagedObject, const HypData** valueHypData);

    ManagedGuid m_guid;
    ManagedAttributeSet m_attributes;
};

} // namespace hyperion::dotnet
