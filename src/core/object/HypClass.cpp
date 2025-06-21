/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion {

#pragma region Helpers

const HypClass* GetClass(TypeID type_id)
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}

const HypClass* GetClass(WeakName type_name)
{
    return HypClassRegistry::GetInstance().GetClass(type_name);
}

const HypEnum* GetEnum(TypeID type_id)
{
    return HypClassRegistry::GetInstance().GetEnum(type_id);
}

const HypEnum* GetEnum(WeakName type_name)
{
    return HypClassRegistry::GetInstance().GetEnum(type_name);
}

bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id)
{
    if (!hyp_class)
    {
        return false;
    }

    if (hyp_class->GetTypeID() == type_id)
    {
        return true;
    }

    const HypClass* other_hyp_class = GetClass(type_id);

    if (other_hyp_class != nullptr)
    {
        if (other_hyp_class->GetStaticIndex() != -1)
        {
            return uint32(other_hyp_class->GetStaticIndex() - hyp_class->GetStaticIndex()) <= hyp_class->GetNumDescendants();
        }

        // Try to get the initializer. If we can get it, use the instance class rather than just the class for the type ID.
        if (const IHypObjectInitializer* initializer = other_hyp_class->GetObjectInitializer(ptr))
        {
            other_hyp_class = initializer->GetClass();
        }
    }

    while (other_hyp_class != nullptr)
    {
        if (other_hyp_class == hyp_class)
        {
            return true;
        }

        other_hyp_class = other_hyp_class->GetParent();
    }

    return false;
}

bool IsInstanceOfHypClass(const HypClass* hyp_class, const HypClass* instance_hyp_class)
{
    if (!hyp_class || !instance_hyp_class)
    {
        return false;
    }

    if (instance_hyp_class->GetStaticIndex() != -1)
    {
        return uint32(instance_hyp_class->GetStaticIndex() - hyp_class->GetStaticIndex()) <= hyp_class->GetNumDescendants();
    }

    do
    {
        if (instance_hyp_class == hyp_class)
        {
            return true;
        }

        instance_hyp_class = instance_hyp_class->GetParent();
    }
    while (instance_hyp_class != nullptr);

    return false;
}

#pragma endregion Helpers

#pragma region HypClassMemberIterator

HypClassMemberIterator::HypClassMemberIterator(const HypClass* hyp_class, EnumFlags<HypMemberType> member_types, Phase phase)
    : m_member_types(member_types),
      m_phase(phase),
      m_target(hyp_class),
      m_current_index(0),
      m_current_value(nullptr)
{
    Advance();
}

void HypClassMemberIterator::Advance()
{
    // HYP_LOG(Object, Debug, "Iterating class {} members: {}, parent = {}, index = {}", target->GetName(), m_phase,
    //     target->GetParent() ? target->GetParent()->GetName().LookupString() : "null", m_current_index);

    if (!m_target)
    {
        return;
    }

    if (m_phase == Phase::MAX)
    {
        m_target = m_target->GetParent();
        m_current_index = 0;
        m_current_value = nullptr;

        if (m_target)
        {
            m_phase = Phase(0);
        }
        else
        {
            return;
        }
    }

    switch (m_phase)
    {
    case Phase::ITERATE_CONSTANTS:
        if ((m_member_types & HypMemberType::TYPE_CONSTANT) && m_current_index < m_target->GetConstants().Size())
        {
            m_current_value = m_target->GetConstants()[m_current_index++];
        }
        else
        {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_PROPERTIES:
        if ((m_member_types & HypMemberType::TYPE_PROPERTY) && m_current_index < m_target->GetProperties().Size())
        {
            m_current_value = m_target->GetProperties()[m_current_index++];
        }
        else
        {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_METHODS:
        if ((m_member_types & HypMemberType::TYPE_METHOD) && m_current_index < m_target->GetMethods().Size())
        {
            m_current_value = m_target->GetMethods()[m_current_index++];
        }
        else
        {
            m_phase = NextPhase(m_member_types, m_phase);
            m_current_index = 0;
            m_current_value = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_FIELDS:
        if ((m_member_types & HypMemberType::TYPE_FIELD) && m_current_index < m_target->GetFields().Size())
        {
            m_current_value = m_target->GetFields()[m_current_index++];
        }
        else
        {
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

HypClass::HypClass(TypeID type_id, Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : m_type_id(type_id),
      m_name(name),
      m_static_index(static_index),
      m_num_descendants(num_descendants),
      m_parent_name(parent_name),
      m_parent(nullptr),
      m_attributes(attributes),
      m_flags(flags),
      m_size(0),
      m_alignment(0),
      m_serialization_mode(HypClassSerializationMode::DEFAULT)
{
    if (bool(m_attributes["abstract"]))
    {
        m_flags |= HypClassFlags::ABSTRACT;
    }

    // initialize properties containers
    for (HypMember& member : members)
    {
        if (HypProperty* property = member.value.TryGet<HypProperty>())
        {
            HypProperty* property_ptr = new HypProperty(std::move(*property));

#ifdef HYP_DEBUG_MODE
            property_ptr->m_getter.type_info.target_type_id = type_id;
            property_ptr->m_setter.type_info.target_type_id = type_id;
#endif

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->GetName(), property_ptr);
        }
        else if (HypMethod* method = member.value.TryGet<HypMethod>())
        {
            HypMethod* method_ptr = new HypMethod(std::move(*method));

            m_methods.PushBack(method_ptr);
            m_methods_by_name.Set(method_ptr->GetName(), method_ptr);
        }
        else if (HypField* field = member.value.TryGet<HypField>())
        {
            HypField* field_ptr = new HypField(std::move(*field));

            m_fields.PushBack(field_ptr);
            m_fields_by_name.Set(field_ptr->GetName(), field_ptr);
        }
        else if (HypConstant* constant = member.value.TryGet<HypConstant>())
        {
            HypConstant* constant_ptr = new HypConstant(std::move(*constant));

            m_constants.PushBack(constant_ptr);
            m_constants_by_name.Set(constant_ptr->GetName(), constant_ptr);
        }
        else
        {
            HYP_FAIL("Invalid member");
        }
    }
}

HypClass::~HypClass()
{
    for (HypProperty* property_ptr : m_properties)
    {
        delete property_ptr;
    }

    for (HypMethod* method_ptr : m_methods)
    {
        delete method_ptr;
    }

    for (HypField* field_ptr : m_fields)
    {
        delete field_ptr;
    }

    for (HypConstant* constant_ptr : m_constants)
    {
        delete constant_ptr;
    }
}

void HypClass::Initialize()
{
    m_serialization_mode = HypClassSerializationMode::DEFAULT;

    if (const HypClassAttributeValue& serialize_attribute = GetAttribute("serialize"))
    {
        if (serialize_attribute.IsString())
        {
            m_serialization_mode = HypClassSerializationMode::NONE;

            const String string_value = serialize_attribute.GetString().ToLower();

            if (string_value == "bitwise")
            {
                if (!IsPOD())
                {
                    HYP_FAIL("Cannot use \"bitwise\" serialization mode for non-POD type: %s", m_name.LookupString());
                }

                m_serialization_mode = HypClassSerializationMode::BITWISE | HypClassSerializationMode::USE_MARSHAL_CLASS;
            }
            else
            {
                HYP_FAIL("Unknown serialization mode: %s", string_value.Data());
            }
        }
        else if (!serialize_attribute.GetBool())
        {
            m_serialization_mode = HypClassSerializationMode::NONE;
        }
    }

    // Disable USE_MARSHAL_CLASS if no marshal is registered by the time this HypClass is initialized
    if (m_serialization_mode & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetTypeID(), /* allow_fallback */ false);

        if (!marshal)
        {
            m_serialization_mode &= ~HypClassSerializationMode::USE_MARSHAL_CLASS;
        }
    }

    if (m_parent_name.IsValid())
    {
        if (!m_parent)
        {
            m_parent = GetClass(m_parent_name);
        }

        AssertThrowMsg(m_parent != nullptr, "Invalid parent class: %s", m_parent_name.LookupString());
    }

    HYP_LOG(Object, Info, "Initializing HypClass \"{}\"", m_name);

    // Build properties from `Property=` attributes on methods and fields
    Array<Pair<String, Array<IHypMember*>>> properties_to_build;

    for (IHypMember& member : GetMembers(false))
    {
        if (const HypClassAttributeValue& attr = member.GetAttribute("property"))
        {
            const String& attr_string = attr.GetString();

            auto properties_to_build_it = properties_to_build.FindIf([&attr_string](const auto& item)
                {
                    return item.first == attr_string;
                });

            if (properties_to_build_it == properties_to_build.End())
            {
                properties_to_build_it = &properties_to_build.EmplaceBack(attr_string, Array<IHypMember*> {});
            }

            properties_to_build_it->second.PushBack(&member);
        }
    }

    for (const Pair<String, Array<IHypMember*>>& it : properties_to_build)
    {
        if (it.second.Empty())
        {
            continue;
        }

        const auto find_field_it = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_FIELD;
            });

        if (find_field_it != it.second.End())
        {
            HypProperty* property_ptr = new HypProperty(HypProperty::MakeHypProperty(static_cast<HypField*>(*find_field_it)));

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->GetName(), property_ptr);

            continue;
        }

        const auto find_getter_it = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_METHOD
                    && static_cast<HypMethod*>(member)->GetParameters().Size() == 1;
            });

        const auto find_setter_it = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_METHOD
                    && static_cast<HypMethod*>(member)->GetParameters().Size() == 2;
            });

        if (find_getter_it != it.second.End() || find_setter_it != it.second.End())
        {
            HypProperty* property_ptr = new HypProperty(HypProperty::MakeHypProperty(
                find_getter_it != it.second.End() ? static_cast<HypMethod*>(*find_getter_it) : nullptr,
                find_setter_it != it.second.End() ? static_cast<HypMethod*>(*find_setter_it) : nullptr));

            m_properties.PushBack(property_ptr);
            m_properties_by_name.Set(property_ptr->GetName(), property_ptr);

            continue;
        }

        HYP_FAIL("Invalid property definition for \"%s\": Must be HYP_FIELD() or getter/setter pair of HYP_METHOD()", it.first.Data());
    }
}

bool HypClass::CanSerialize() const
{
    if (m_serialization_mode == HypClassSerializationMode::NONE)
    {
        return false;
    }

    if (m_serialization_mode & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        return true;
    }

    if (m_serialization_mode & HypClassSerializationMode::MEMBERWISE)
    {
        return true;
    }

    if (m_serialization_mode & HypClassSerializationMode::BITWISE)
    {
        if (IsStructType())
        {
            return true;
        }
    }

    return false;
}

IHypMember* HypClass::GetMember(WeakName name) const
{
    if (HypProperty* property = GetProperty(name))
    {
        return property;
    }

    if (HypMethod* method = GetMethod(name))
    {
        return method;
    }

    if (HypField* field = GetField(name))
    {
        return field;
    }

    if (const HypClass* parent = GetParent())
    {
        return parent->GetMember(name);
    }

    return nullptr;
}

HypProperty* HypClass::GetProperty(WeakName name) const
{
    const auto it = m_properties_by_name.FindAs(name);

    if (it == m_properties_by_name.End())
    {
        if (const HypClass* parent = GetParent())
        {
            return parent->GetProperty(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypProperty*> HypClass::GetPropertiesInherited() const
{
    if (const HypClass* parent = GetParent())
    {
        FlatSet<HypProperty*> properties { GetProperties().Begin(), GetProperties().End() };

        Array<HypProperty*> inherited_properties = parent->GetPropertiesInherited();

        for (HypProperty* property : inherited_properties)
        {
            properties.Insert(property);
        }

        return properties.ToArray();
    }

    return m_properties;
}

HypMethod* HypClass::GetMethod(WeakName name) const
{
    const auto it = m_methods_by_name.FindAs(name);

    if (it == m_methods_by_name.End())
    {
        if (const HypClass* parent = GetParent())
        {
            return parent->GetMethod(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypMethod*> HypClass::GetMethodsInherited() const
{
    if (const HypClass* parent = GetParent())
    {
        FlatSet<HypMethod*> methods { m_methods.Begin(), m_methods.End() };

        Array<HypMethod*> inherited_methods = parent->GetMethodsInherited();

        for (HypMethod* method : inherited_methods)
        {
            methods.Insert(method);
        }

        return methods.ToArray();
    }

    return m_methods;
}

HypField* HypClass::GetField(WeakName name) const
{
    const auto it = m_fields_by_name.FindAs(name);

    if (it == m_fields_by_name.End())
    {
        if (const HypClass* parent = GetParent())
        {
            return parent->GetField(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypField*> HypClass::GetFieldsInherited() const
{
    if (const HypClass* parent = GetParent())
    {
        FlatSet<HypField*> fields { m_fields.Begin(), m_fields.End() };

        Array<HypField*> inherited_fields = parent->GetFieldsInherited();

        for (HypField* field : inherited_fields)
        {
            fields.Insert(field);
        }

        return fields.ToArray();
    }

    return m_fields;
}

HypConstant* HypClass::GetConstant(WeakName name) const
{
    const auto it = m_constants_by_name.FindAs(name);

    if (it == m_constants_by_name.End())
    {
        if (const HypClass* parent = GetParent())
        {
            return parent->GetConstant(name);
        }

        return nullptr;
    }

    return it->second;
}

Array<HypConstant*> HypClass::GetConstantsInherited() const
{
    if (const HypClass* parent = GetParent())
    {
        FlatSet<HypConstant*> constants { m_constants.Begin(), m_constants.End() };

        Array<HypConstant*> inherited_constants = parent->GetConstantsInherited();

        for (HypConstant* constant : inherited_constants)
        {
            constants.Insert(constant);
        }

        return constants.ToArray();
    }

    return m_constants;
}

bool HypClass::GetManagedObjectFromObjectInitializer(const IHypObjectInitializer* object_initializer, dotnet::ObjectReference& out_object_reference)
{
    if (!object_initializer)
    {
        HYP_LOG(Object, Error, "Cannot get managed object from null object initializer");

        return false;
    }

    if (!object_initializer->GetManagedObjectResource())
    {
        HYP_LOG(Object, Error, "Cannot get managed object from object initializer without a managed object resource");

        return false;
    }

    TResourceHandle<ManagedObjectResource> resource_handle(*object_initializer->GetManagedObjectResource());

    out_object_reference = resource_handle->GetManagedObject()->GetObjectReference();

    return true;
}

bool HypClass::IsDerivedFrom(const HypClass* other) const
{
    if (other == nullptr)
    {
        return false;
    }

    if (this == other)
    {
        return true;
    }

    const HypClass* current = this;

    while (current != nullptr)
    {
        if (current->m_parent == other)
        {
            return true;
        }

        current = current->m_parent;
    }

    return false;
}

#pragma endregion HypClass

} // namespace hyperion