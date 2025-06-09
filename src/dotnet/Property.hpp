/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_PROPERTY_HPP
#define HYPERION_DOTNET_PROPERTY_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/Helpers.hpp>
#include <dotnet/Attribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

class Object;

class Property
{
public:
    Property() = default;

    Property(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    Property(ManagedGuid guid, AttributeSet&& attributes)
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

    HYP_FORCE_INLINE const AttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    template <class ReturnType>
    ReturnType InvokeGetter(const Object* object_ptr)
    {
        HypData return_hyp_data;
        InvokeGetter_Internal(object_ptr, &return_hyp_data);

        return std::move(return_hyp_data.Get<ReturnType>());
    }

    template <class PropertyType>
    void InvokeSetter(const Object* object_ptr, PropertyType&& value)
    {
        HypData value_hyp_data(TransformArgument<PropertyType> {}(std::forward<PropertyType>(value)));
        const HypData* value_hyp_data_ptr = &value_hyp_data;

        return InvokeSetter_Internal(object_ptr, &value_hyp_data_ptr);
    }

private:
    void InvokeGetter_Internal(const Object* object_ptr, HypData* out_return_hyp_data);
    void InvokeSetter_Internal(const Object* object_ptr, const HypData** value_hyp_data);

    ManagedGuid m_guid;
    AttributeSet m_attributes;
};

} // namespace hyperion::dotnet

#endif