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
    Property()                                      = default;

    Property(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    Property(ManagedGuid guid, AttributeSet &&attributes)
        : m_guid(guid),
          m_attributes(std::move(attributes))
    {
    }

    Property(const Property &other)                 = delete;
    Property &operator=(const Property &other)      = delete;

    Property(Property &&other) noexcept             = default;
    Property &operator=(Property &&other) noexcept  = default;

    ~Property()                                     = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
        { return m_guid; }

    HYP_FORCE_INLINE const AttributeSet &GetAttributes() const
        { return m_attributes; }

    template <class ReturnType>
    ReturnType InvokeGetter(const Object *object_ptr)
    {
        ValueStorage<ReturnType> result_storage;
        InvokeGetter_Internal(object_ptr, result_storage.GetPointer());
        return std::move(result_storage.Get());
    }

    template <class PropertyType>
    void InvokeSetter(const Object *object_ptr, PropertyType &&value)
    {
        return InvokeSetter_Internal(object_ptr, detail::TransformArgument<PropertyType>{}(std::forward<PropertyType>(value)));
    }

private:
    void InvokeGetter_Internal(const Object *object_ptr, void *return_value_vptr);
    void InvokeSetter_Internal(const Object *object_ptr, void *value_vptr);

    ManagedGuid     m_guid;
    AttributeSet    m_attributes;
};

} // namespace hyperion::dotnet

#endif