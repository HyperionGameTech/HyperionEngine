/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_METHOD_HPP
#define HYPERION_DOTNET_METHOD_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/Attribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

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