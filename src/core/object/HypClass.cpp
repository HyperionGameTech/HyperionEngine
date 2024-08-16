/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClass

HypClass::HypClass(TypeID type_id, Name name, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : m_type_id(type_id),
      m_name(name),
      m_flags(flags)
{
    // initialize properties containers
    for (HypMember &member : members) {
        if (HypProperty *property = member.value.TryGet<HypProperty>()) {
            HypProperty *property_ptr = new HypProperty(std::move(*property));

#ifdef HYP_DEBUG_MODE
            property_ptr->getter.type_info.target_type_id = type_id;
            property_ptr->setter.type_info.target_type_id = type_id;
#endif

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->name, property_ptr);
        } else if (HypMethod *method = member.value.TryGet<HypMethod>()) {
            HypMethod *method_ptr = new HypMethod(std::move(*method));

            m_methods.PushBack(method_ptr);
            m_methods_by_name.Set(method_ptr->name, method_ptr);
        } else if (HypField *field = member.value.TryGet<HypField>()) {
            HypField *field_ptr = new HypField(std::move(*field));

            m_fields.PushBack(field_ptr);
            m_fields_by_name.Set(field_ptr->name, field_ptr);
        } else {
            HYP_FAIL("Invalid member");
        }
    }
}

HypClass::~HypClass()
{
    for (HypProperty *property_ptr : m_properties) {
        delete property_ptr;
    }

    for (HypMethod *method_ptr : m_methods) {
        delete method_ptr;
    }

    for (HypField *field_ptr : m_fields) {
        delete field_ptr;
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

HypMethod *HypClass::GetMethod(WeakName name) const
{
    const auto it = m_methods_by_name.FindAs(name);

    if (it == m_methods_by_name.End()) {
        return nullptr;
    }

    return it->second;
}

HypField *HypClass::GetField(WeakName name) const
{
    const auto it = m_fields_by_name.FindAs(name);

    if (it == m_fields_by_name.End()) {
        return nullptr;
    }

    return it->second;
}

dotnet::Class *HypClass::GetManagedClass() const
{
    return HypClassRegistry::GetInstance().GetManagedClass(this);
}

bool HypClass::GetManagedObjectFromObjectInitializer(const IHypObjectInitializer *object_initializer, dotnet::ObjectReference &out_object_reference)
{
    if (!object_initializer) {
        HYP_LOG(Object, LogLevel::ERR, "Could not get managed object for instance of HypClass; No object initializer");

        return false;
    }

    if (!object_initializer->GetManagedObject()) {
        HYP_LOG(Object, LogLevel::ERR, "Could not get managed object for instance of HypClass; No managed object assigned");

        return false;
    }

    out_object_reference = object_initializer->GetManagedObject()->GetObjectReference();

    return true;
}

#pragma endregion HypClass

} // namespace hyperion