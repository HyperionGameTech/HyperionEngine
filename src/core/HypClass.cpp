/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClass

HypClass::HypClass(TypeID type_id, Span<HypClassProperty> properties)
    : m_type_id(type_id)
{
    // initialize properties containers
    for (HypClassProperty &property : properties) {
        HypClassProperty *property_ptr = new HypClassProperty(std::move(property));

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        property_ptr->getter.debug_info.target_type_id = type_id;
        property_ptr->setter.debug_info.target_type_id = type_id;
#endif

        m_properties.PushBack(property_ptr);
        m_properties_by_name.Set(property_ptr->name, property_ptr);
    }
}

HypClass::~HypClass()
{
    for (HypClassProperty *property_ptr : m_properties) {
        delete property_ptr;
    }
}

HypClassProperty *HypClass::GetProperty(WeakName name) const
{
    const auto it = m_properties_by_name.FindAs(name);

    if (it == m_properties_by_name.End()) {
        return nullptr;
    }

    return it->second;
}

#pragma endregion HypClass

} // namespace hyperion