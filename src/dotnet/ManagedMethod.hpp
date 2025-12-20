/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <dotnet/ManagedAttribute.hpp>
#include <dotnet/Helpers.hpp>

#include <dotnet/Types.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion::dotnet {

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

    HYP_FORCE_INLINE void Invoke(ObjectReference* thisObjectReference, const BoxedValue** argsHypData, BoxedValue* outReturnHypData) const
    {
        m_invokeFptr(thisObjectReference, argsHypData, outReturnHypData);
    }

private:
    ManagedGuid m_guid;
    InvokeMethodFunction m_invokeFptr;
    ManagedAttributeSet m_attributes;
};

} // namespace hyperion::dotnet
