/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_METHOD_HPP
#define HYPERION_DOTNET_METHOD_HPP

#include <dotnet/Attribute.hpp>
#include <dotnet/Helpers.hpp>

#include <dotnet/Types.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion::dotnet {

struct ObjectReference;

class Method
{
public:
    Method()
        : m_invoke_fptr(nullptr)
    {
    }

    Method(ManagedGuid guid, InvokeMethodFunction invoke_fptr)
        : m_guid(guid),
          m_invoke_fptr(invoke_fptr)
    {
    }

    Method(ManagedGuid guid, InvokeMethodFunction invoke_fptr, AttributeSet&& attributes)
        : m_guid(guid),
          m_invoke_fptr(invoke_fptr),
          m_attributes(std::move(attributes))
    {
    }

    Method(const Method& other) = delete;
    Method& operator=(const Method& other) = delete;

    Method(Method&& other) noexcept = default;
    Method& operator=(Method&& other) noexcept = default;

    ~Method() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE InvokeMethodFunction GetFunctionPointer() const
    {
        return m_invoke_fptr;
    }

    HYP_FORCE_INLINE const AttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE void Invoke(ObjectReference* this_object_reference, const HypData** args_hyp_data, HypData* out_return_hyp_data) const
    {
        AssertDebug(m_invoke_fptr != nullptr);
        m_invoke_fptr(this_object_reference, args_hyp_data, out_return_hyp_data);
    }

private:
    ManagedGuid m_guid;
    InvokeMethodFunction m_invoke_fptr;
    AttributeSet m_attributes;
};

} // namespace hyperion::dotnet

#endif