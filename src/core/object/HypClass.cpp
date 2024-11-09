/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypObject.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion {

#pragma region HypClassMemberIterator

HypClassMemberIterator::HypClassMemberIterator(const HypClass *hyp_class, EnumFlags<HypMemberType> member_types, Phase phase)
    : m_member_types(member_types),
      m_phase(phase),
      m_hyp_class(hyp_class),
      m_iterating_parent(nullptr),
      m_current_index(0),
      m_current_value(nullptr)
{
    Advance();
}

void HypClassMemberIterator::Advance()
{
    const HypClass *target = m_iterating_parent != nullptr ? m_iterating_parent : m_hyp_class;

    if (m_phase == Phase::MAX) {
        if ((target = m_iterating_parent = target->GetParent())) {
            m_phase = NextPhase(m_member_types, m_phase);
        }
    }

    if (!target) {
        return;
    }

    switch (m_phase) {
    case Phase::ITERATE_PROPERTIES:
        if (m_current_index < target->GetProperties().Size()) {
            m_current_value = target->GetProperties()[m_current_index++];
        } else {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_METHODS:
        if (m_current_index < target->GetMethods().Size()) {
            m_current_value = target->GetMethods()[m_current_index++];
        } else {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;
            
            Advance();
        }

        break;
    case Phase::ITERATE_FIELDS:
        if (m_current_index < target->GetFields().Size()) {
            m_current_value = target->GetFields()[m_current_index++];
        } else {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;

            Advance();
        }

        break;
    default:
        break;
    }
}

#pragma endregion HypClassMemberIterator

#pragma region HypClass

HypClass::HypClass(TypeID type_id, Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : m_type_id(type_id),
      m_name(name),
      m_parent_name(parent_name),
      m_parent(nullptr),
      m_attributes(attributes),
      m_flags(flags),
      m_serialization_mode(HypClassSerializationMode::DEFAULT)
{
    if (bool(m_attributes["abstract"])) {
        m_flags |= HypClassFlags::ABSTRACT;
    }

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

void HypClass::Initialize()
{
    m_serialization_mode = HypClassSerializationMode::DEFAULT;

    if (const HypClassAttributeValue &serialize_attribute = GetAttribute("serialize")) {
        if (serialize_attribute.IsString()) {
            m_serialization_mode = HypClassSerializationMode::NONE;

            const String string_value = serialize_attribute.GetString().ToLower();

            if (string_value == "bitwise") {
                if (!IsPOD()) {
                    HYP_FAIL("Cannot use \"bitwise\" serialization mode for non-POD type: %s", m_name.LookupString());
                }

                m_serialization_mode = HypClassSerializationMode::BITWISE | HypClassSerializationMode::USE_MARSHAL_CLASS;
            } else {
                m_serialization_mode = HypClassSerializationMode::MEMBERWISE | HypClassSerializationMode::USE_MARSHAL_CLASS;
            }
        } else if (!serialize_attribute.GetBool()) {
            m_serialization_mode = HypClassSerializationMode::NONE;
        }
    }

    // Disable USE_MARSHAL_CLASS if no marshal is registered by the time this HypClass is initialized
    if (m_serialization_mode & HypClassSerializationMode::USE_MARSHAL_CLASS) {
        fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(GetTypeID(), /* allow_fallback */ false);

        if (!marshal) {
            m_serialization_mode &= ~HypClassSerializationMode::USE_MARSHAL_CLASS;
        }
    }

    if (m_parent_name.IsValid()) {
        m_parent = GetClass(m_parent_name);
        AssertThrowMsg(m_parent != nullptr, "Invalid parent class: %s", m_parent_name.LookupString());
    }

    HYP_LOG(Object, LogLevel::INFO, "Initializing HypClass \"{}\"", m_name);

    // Build properties from `Property=` attributes on methods and fields
    Array<Pair<String, Array<IHypMember *>>> properties_to_build;

    for (IHypMember &member : GetMembers(false)) {
        if (const HypClassAttributeValue &attr = member.GetAttribute("property")) {
            const String &attr_string = attr.GetString();

            auto properties_to_build_it = properties_to_build.FindIf([&attr_string](const auto &item)
            {
                return item.first == attr_string;
            });

            if (properties_to_build_it == properties_to_build.End()) {
                properties_to_build_it = &properties_to_build.EmplaceBack(attr_string, Array<IHypMember *> { });
            }

            properties_to_build_it->second.PushBack(&member);
        }
    }

    for (const Pair<String, Array<IHypMember *>> &it : properties_to_build) {
        if (it.second.Empty()) {
            continue;
        }

        const auto find_field_it = it.second.FindIf([](IHypMember *member)
        {
            return member->GetMemberType() == HypMemberType::TYPE_FIELD;
        });

        if (find_field_it != it.second.End()) {
            HypProperty *property_ptr = new HypProperty(HypProperty::MakeHypProperty(static_cast<HypField *>(*find_field_it)));

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->name, property_ptr);

            HYP_LOG(Object, LogLevel::DEBUG, "Built property \"{}\" on HypClass \"{}\" from field", property_ptr->name, m_name);

            continue;
        }

        const auto find_getter_it = it.second.FindIf([](IHypMember *member)
        {
            return member->GetMemberType() == HypMemberType::TYPE_METHOD
                && static_cast<HypMethod *>(member)->params.Size() == 1;
        });

        const auto find_setter_it = it.second.FindIf([](IHypMember *member)
        {
            return member->GetMemberType() == HypMemberType::TYPE_METHOD
                && static_cast<HypMethod *>(member)->params.Size() == 2;
        });

        if (find_getter_it != it.second.End() || find_setter_it != it.second.End()) {
            HypProperty *property_ptr = new HypProperty(HypProperty::MakeHypProperty(
                find_getter_it != it.second.End() ? static_cast<HypMethod *>(*find_getter_it) : nullptr,
                find_setter_it != it.second.End() ? static_cast<HypMethod *>(*find_setter_it) : nullptr
            ));

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->name, property_ptr);

            HYP_LOG(Object, LogLevel::DEBUG, "Built property \"{}\" on HypClass \"{}\" from methods", property_ptr->name, m_name);

            continue;
        }

        HYP_FAIL("Invalid property definition for \"%s\": Must be HYP_FIELD() or getter/setter pair of HYP_METHOD()", it.first.Data());
    }
}

bool HypClass::CanSerialize() const
{
    if (m_serialization_mode == HypClassSerializationMode::NONE) {
        return false;
    }

    if (m_serialization_mode & HypClassSerializationMode::USE_MARSHAL_CLASS) {
        return true;
    }

    if (m_serialization_mode & HypClassSerializationMode::MEMBERWISE) {
        return true;
    }

    if (m_serialization_mode & HypClassSerializationMode::BITWISE) {
        if (IsStructType()) {
            return true;
        }
    }

    return false;
}

IHypMember *HypClass::GetMember(WeakName name) const
{
    if (HypProperty *property = GetProperty(name)) {
        return property;
    }

    if (HypMethod *method = GetMethod(name)) {
        return method;
    }

    if (HypField *field = GetField(name)) {
        return field;
    }

    if (const HypClass *parent = GetParent()) {
        return parent->GetMember(name);
    }

    return nullptr;
}

HypProperty *HypClass::GetProperty(WeakName name) const
{
    const auto it = m_properties_by_name.FindAs(name);

    if (it == m_properties_by_name.End()) {
        if (const HypClass *parent = GetParent()) {
            return parent->GetProperty(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypProperty *> HypClass::GetPropertiesInherited() const
{
    if (const HypClass *parent = GetParent()) {
        FlatSet<HypProperty *> properties { GetProperties().Begin(), GetProperties().End() };

        Array<HypProperty *> inherited_properties = parent->GetPropertiesInherited();

        for (HypProperty *property : inherited_properties) {
            properties.Insert(property);
        }

        return properties.ToArray();
    }

    return m_properties;
}

HypMethod *HypClass::GetMethod(WeakName name) const
{
    const auto it = m_methods_by_name.FindAs(name);

    if (it == m_methods_by_name.End()) {
        if (const HypClass *parent = GetParent()) {
            return parent->GetMethod(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypMethod *> HypClass::GetMethodsInherited() const
{
    if (const HypClass *parent = GetParent()) {
        FlatSet<HypMethod *> methods { m_methods.Begin(), m_methods.End() };

        Array<HypMethod *> inherited_methods = parent->GetMethodsInherited();

        for (HypMethod *method : inherited_methods) {
            methods.Insert(method);
        }

        return methods.ToArray();
    }

    return m_methods;
}

HypField *HypClass::GetField(WeakName name) const
{
    const auto it = m_fields_by_name.FindAs(name);

    if (it == m_fields_by_name.End()) {
        if (const HypClass *parent = GetParent()) {
            return parent->GetField(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypField *> HypClass::GetFieldsInherited() const
{
    if (const HypClass *parent = GetParent()) {
        FlatSet<HypField *> fields { m_fields.Begin(), m_fields.End() };

        Array<HypField *> inherited_fields = parent->GetFieldsInherited();

        for (HypField *field : inherited_fields) {
            fields.Insert(field);
        }

        return fields.ToArray();
    }

    return m_fields;
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