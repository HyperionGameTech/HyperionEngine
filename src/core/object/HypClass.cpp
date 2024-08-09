/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClass

HypClass::HypClass(TypeID type_id, EnumFlags<HypClassFlags> flags, Span<HypProperty> properties)
    : m_type_id(type_id),
      m_flags(flags)
{
    // initialize properties containers
    for (HypProperty &property : properties) {
        HypProperty *property_ptr = new HypProperty(std::move(property));

#ifdef HYP_DEBUG_MODE
        property_ptr->getter.type_info.target_type_id = type_id;
        property_ptr->setter.type_info.target_type_id = type_id;
#endif

        m_properties.PushBack(property_ptr);
        m_properties_by_name.Set(property_ptr->name, property_ptr);
    }
}

HypClass::~HypClass()
{
    for (HypProperty *property_ptr : m_properties) {
        delete property_ptr;
    }
}

HypProperty *HypClass::GetProperty(WeakName name) const
{
    const auto it = m_properties_by_name.FindAs(name);

    if (it == m_properties_by_name.End()) {
        return nullptr;
    }

    return it->second;
}

dotnet::Class *HypClass::GetManagedClass() const
{
    return HypClassRegistry::GetInstance().GetManagedClass(this);
}

#pragma endregion HypClass

} // namespace hyperion