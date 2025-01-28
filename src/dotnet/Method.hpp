/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_METHOD_HPP
#define HYPERION_DOTNET_METHOD_HPP

#include <core/object/HypData.hpp>

#include <dotnet/Attribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion::dotnet {

// Conditionally construct or reference existing HypData
template <class T>
const HypData *SetArg_HypData(HypData *arr, SizeType index, T &&arg)
{
    if constexpr (is_hypdata_v<T>) {
        return &arg;
    } else {
        new (&arr[index]) HypData(std::forward<T>(arg));
        return &arr[index];
    }
}

// Expand over each argument to fill args_array and args_array_ptr
template <class... Args, SizeType... Indices>
void FillArgs_HypData(std::index_sequence<Indices...>, HypData *arr, const HypData *(&array_ptr)[sizeof...(Args)], Args &&... args)
{
    ((array_ptr[Indices] = SetArg_HypData(arr, Indices, std::forward<Args>(args))), ...);
}

class Method
{
public:
    Method()                                    = default;

    Method(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    Method(ManagedGuid guid, AttributeSet &&attributes)
        : m_guid(guid),
          m_attributes(std::move(attributes))
    {
    }

    Method(const Method &other)                 = delete;
    Method &operator=(const Method &other)      = delete;

    Method(Method &&other) noexcept             = default;
    Method &operator=(Method &&other) noexcept  = default;

    ~Method()                                   = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
        { return m_guid; }

    HYP_FORCE_INLINE const AttributeSet &GetAttributes() const
        { return m_attributes; }

private:
    ManagedGuid     m_guid;
    AttributeSet    m_attributes;
};

} // namespace hyperion::dotnet

#endif