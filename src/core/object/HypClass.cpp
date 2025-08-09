/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/ThreadLocalStorage.hpp>

#include <dotnet/Object.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion {

#pragma region Helpers

const HypClass* GetClass(TypeId typeId)
{
    return HypClassRegistry::GetInstance().GetClass(typeId);
}

const HypClass* GetClass(WeakName typeName)
{
    return HypClassRegistry::GetInstance().GetClass(typeName);
}

const HypEnum* GetEnum(TypeId typeId)
{
    return HypClassRegistry::GetInstance().GetEnum(typeId);
}

const HypEnum* GetEnum(WeakName typeName)
{
    return HypClassRegistry::GetInstance().GetEnum(typeName);
}

bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId)
{
    if (!hypClass)
    {
        return false;
    }

    if (hypClass->GetTypeId() == typeId)
    {
        return true;
    }

    const HypClass* otherHypClass = GetClass(typeId);

    if (otherHypClass != nullptr)
    {
        // fast path
        if (otherHypClass->GetStaticIndex() >= 0)
        {
            return uint32(otherHypClass->GetStaticIndex() - hypClass->GetStaticIndex()) <= hypClass->GetNumDescendants();
        }

        // Try to get the initializer. If we can get it, use the instance class rather than just the class for the type Id.
        if (const IHypObjectInitializer* initializer = otherHypClass->GetObjectInitializer(ptr))
        {
            otherHypClass = initializer->GetClass();
        }
    }

    // slow path
    while (otherHypClass != nullptr)
    {
        if (otherHypClass == hypClass)
        {
            return true;
        }

        otherHypClass = otherHypClass->GetParent();
    }

    return false;
}

bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass)
{
    if (!hypClass || !instanceHypClass)
    {
        return false;
    }

    // fast path
    if (instanceHypClass->GetStaticIndex() >= 0)
    {
        return uint32(instanceHypClass->GetStaticIndex() - hypClass->GetStaticIndex()) <= hypClass->GetNumDescendants();
    }

    // slow path
    do
    {
        if (instanceHypClass == hypClass)
        {
            return true;
        }

        instanceHypClass = instanceHypClass->GetParent();
    }
    while (instanceHypClass != nullptr);

    return false;
}

int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId)
{
    const HypClass* base = GetClass(baseTypeId);
    if (!base)
    {
        return -2;
    }

    const HypClass* subclass = GetClass(subclassTypeId);

    if (!subclass)
    {
        return -2;
    }

    const int subclassStaticIndex = subclass->GetStaticIndex();
    if (subclassStaticIndex < 0)
    {
        return -2; // subclass is not a static class
    }

    const int baseStaticIndex = base->GetStaticIndex();

    if (subclassStaticIndex == baseStaticIndex)
    {
        return -1; // base class returns -1 for static index
    }

    if (uint32(subclassStaticIndex - base->GetStaticIndex()) <= base->GetNumDescendants())
    {
        // subtract one to get subclass index (has to fit within base's num descendants)
        return subclassStaticIndex - base->GetStaticIndex() - 1;
    }

    return -2;
}

SizeType GetNumDescendants(TypeId typeId)
{
    const HypClass* base = GetClass(typeId);
    if (!base)
    {
        return 0;
    }

    return base->GetNumDescendants();
}

using FormattedStringMap = HashMap<TypeId, String, HashTable_DynamicNodeAllocator<KeyValuePair<TypeId, String>>>;
thread_local FormattedStringMap* g_formattedStringMap;

const char* LookupTypeName(TypeId typeId)
{
    const HypClass* hypClass = GetClass(typeId);
    
    if (hypClass)
    {
        return *hypClass->GetName();
    }
    
    if (!g_formattedStringMap)
    {
        g_formattedStringMap = Threads::CurrentThreadObject()->GetTLS().Alloc<FormattedStringMap>();
        new (g_formattedStringMap) FormattedStringMap();
        
        Threads::CurrentThreadObject()->AtExit([]()
        {
            g_formattedStringMap->~FormattedStringMap();
        });
    }
    
    auto it = g_formattedStringMap->Find(typeId);
    
    if (it == g_formattedStringMap->End())
    {
        it = g_formattedStringMap->Insert(typeId, HYP_FORMAT("TypeId({})", typeId.Value())).first;
    }
    
    return *it->second;
}


#pragma endregion Helpers

#pragma region HypClassMemberIterator

HypClassMemberIterator::HypClassMemberIterator(const HypClass* hypClass, EnumFlags<HypMemberType> memberTypes, Phase phase)
    : m_memberTypes(memberTypes),
      m_phase(phase),
      m_target(hypClass),
      m_currentIndex(0),
      m_currentValue(nullptr)
{
    Advance();
}

void HypClassMemberIterator::Advance()
{
    // HYP_LOG(Object, Debug, "Iterating class {} members: {}, parent = {}, index = {}", target->GetName(), m_phase,
    //     target->GetParent() ? target->GetParent()->GetName().LookupString() : "null", m_currentIndex);

    if (!m_target)
    {
        return;
    }

    if (m_phase == Phase::MAX)
    {
        m_target = m_target->GetParent();
        m_currentIndex = 0;
        m_currentValue = nullptr;

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
        if ((m_memberTypes & HypMemberType::TYPE_CONSTANT) && m_currentIndex < m_target->GetConstants().Size())
        {
            m_currentValue = m_target->GetConstants()[m_currentIndex++];
        }
        else
        {
            m_phase = NextPhase(m_memberTypes, m_phase);
            m_currentIndex = 0;
            m_currentValue = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_PROPERTIES:
        if ((m_memberTypes & HypMemberType::TYPE_PROPERTY) && m_currentIndex < m_target->GetProperties().Size())
        {
            m_currentValue = m_target->GetProperties()[m_currentIndex++];
        }
        else
        {
            m_phase = NextPhase(m_memberTypes, m_phase);
            m_currentIndex = 0;
            m_currentValue = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_METHODS:
        if ((m_memberTypes & HypMemberType::TYPE_METHOD) && m_currentIndex < m_target->GetMethods().Size())
        {
            m_currentValue = m_target->GetMethods()[m_currentIndex++];
        }
        else
        {
            m_phase = NextPhase(m_memberTypes, m_phase);
            m_currentIndex = 0;
            m_currentValue = nullptr;

            Advance();
        }

        break;
    case Phase::ITERATE_FIELDS:
        if ((m_memberTypes & HypMemberType::TYPE_FIELD) && m_currentIndex < m_target->GetFields().Size())
        {
            m_currentValue = m_target->GetFields()[m_currentIndex++];
        }
        else
        {
            m_phase = NextPhase(m_memberTypes, m_phase);
            m_currentIndex = 0;
            m_currentValue = nullptr;

            Advance();
        }

        break;
    default:
        break;
    }
}

#pragma endregion HypClassMemberIterator

#pragma region HypClass

HypClass::HypClass(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : m_typeId(typeId),
      m_name(name),
      m_staticIndex(staticIndex),
      m_numDescendants(numDescendants),
      m_parentName(parentName),
      m_parent(nullptr),
      m_attributes(attributes),
      m_flags(flags),
      m_size(0),
      m_alignment(0),
      m_serializationMode(HypClassSerializationMode::DEFAULT)
{
    if (staticIndex >= 0)
    {
        HYP_CORE_ASSERT(staticIndex < g_maxStaticClassIndex, "Static index %d exceeds maximum static class index %u", staticIndex, g_maxStaticClassIndex);
    }

    if (bool(m_attributes["abstract"]))
    {
        m_flags |= HypClassFlags::ABSTRACT;
    }

    // initialize properties containers
    for (HypMember& member : members)
    {
        if (HypProperty* property = member.value.TryGet<HypProperty>())
        {
            HypProperty* propertyPtr = new HypProperty(std::move(*property));

#ifdef HYP_DEBUG_MODE
            propertyPtr->m_getter.typeInfo.targetTypeId = typeId;
            propertyPtr->m_setter.typeInfo.targetTypeId = typeId;
#endif

            m_properties.PushBack(propertyPtr);
            m_propertiesByName.Set(propertyPtr->GetName(), propertyPtr);
        }
        else if (HypMethod* method = member.value.TryGet<HypMethod>())
        {
            HypMethod* methodPtr = new HypMethod(std::move(*method));

            m_methods.PushBack(methodPtr);
            m_methodsByName.Set(methodPtr->GetName(), methodPtr);
        }
        else if (HypField* field = member.value.TryGet<HypField>())
        {
            HypField* fieldPtr = new HypField(std::move(*field));

            m_fields.PushBack(fieldPtr);
            m_fieldsByName.Set(fieldPtr->GetName(), fieldPtr);
        }
        else if (HypConstant* constant = member.value.TryGet<HypConstant>())
        {
            HypConstant* constantPtr = new HypConstant(std::move(*constant));

            m_constants.PushBack(constantPtr);
            m_constantsByName.Set(constantPtr->GetName(), constantPtr);
        }
        else
        {
            HYP_FAIL("Invalid member");
        }
    }
}

HypClass::~HypClass()
{
    for (HypProperty* propertyPtr : m_properties)
    {
        delete propertyPtr;
    }

    for (HypMethod* methodPtr : m_methods)
    {
        delete methodPtr;
    }

    for (HypField* fieldPtr : m_fields)
    {
        delete fieldPtr;
    }

    for (HypConstant* constantPtr : m_constants)
    {
        delete constantPtr;
    }
}

void HypClass::Initialize()
{
    m_serializationMode = HypClassSerializationMode::DEFAULT;

    if (const HypClassAttributeValue& serializeAttribute = GetAttribute("serialize"))
    {
        if (serializeAttribute.IsString())
        {
            m_serializationMode = HypClassSerializationMode::NONE;

            const String stringValue = serializeAttribute.GetString().ToLower();

            if (stringValue == "bitwise")
            {
                if (!IsPOD())
                {
                    HYP_FAIL("Cannot use \"bitwise\" serialization mode for non-POD type: %s", m_name.LookupString());
                }

                m_serializationMode = HypClassSerializationMode::BITWISE | HypClassSerializationMode::USE_MARSHAL_CLASS;
            }
            else
            {
                HYP_FAIL("Unknown serialization mode: %s", stringValue.Data());
            }
        }
        else if (!serializeAttribute.GetBool())
        {
            m_serializationMode = HypClassSerializationMode::NONE;
        }
    }

    // Disable USE_MARSHAL_CLASS if no marshal is registered by the time this HypClass is initialized
    if (m_serializationMode & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetTypeId(), /* allowFallback */ false);

        if (!marshal)
        {
            m_serializationMode &= ~HypClassSerializationMode::USE_MARSHAL_CLASS;
        }
    }

    if (m_parentName.IsValid())
    {
        if (!m_parent)
        {
            m_parent = GetClass(m_parentName);
        }

        HYP_CORE_ASSERT(m_parent != nullptr, "Invalid parent class: %s", m_parentName.LookupString());
    }

    HYP_LOG(Object, Info, "Initializing HypClass \"{}\"", m_name);

    // Build properties from `Property=` attributes on methods and fields
    Array<Pair<String, Array<IHypMember*>>> propertiesToBuild;

    for (IHypMember& member : GetMembers(false))
    {
        if (const HypClassAttributeValue& attr = member.GetAttribute("property"))
        {
            const String& attrString = attr.GetString();

            auto propertiesToBuildIt = propertiesToBuild.FindIf([&attrString](const auto& item)
                {
                    return item.first == attrString;
                });

            if (propertiesToBuildIt == propertiesToBuild.End())
            {
                propertiesToBuildIt = &propertiesToBuild.EmplaceBack(attrString, Array<IHypMember*> {});
            }

            propertiesToBuildIt->second.PushBack(&member);
        }
    }

    for (const Pair<String, Array<IHypMember*>>& it : propertiesToBuild)
    {
        if (it.second.Empty())
        {
            continue;
        }

        const auto findFieldIt = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_FIELD;
            });

        if (findFieldIt != it.second.End())
        {
            HypProperty* propertyPtr = new HypProperty(HypProperty::MakeHypProperty(static_cast<HypField*>(*findFieldIt)));

            m_properties.PushBack(propertyPtr);
            m_propertiesByName.Set(propertyPtr->GetName(), propertyPtr);

            continue;
        }

        const auto findGetterIt = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_METHOD
                    && static_cast<HypMethod*>(member)->GetParameters().Size() == 1;
            });

        const auto findSetterIt = it.second.FindIf([](IHypMember* member)
            {
                return member->GetMemberType() == HypMemberType::TYPE_METHOD
                    && static_cast<HypMethod*>(member)->GetParameters().Size() == 2;
            });

        if (findGetterIt != it.second.End() || findSetterIt != it.second.End())
        {
            HypProperty* propertyPtr = new HypProperty(HypProperty::MakeHypProperty(
                findGetterIt != it.second.End() ? static_cast<HypMethod*>(*findGetterIt) : nullptr,
                findSetterIt != it.second.End() ? static_cast<HypMethod*>(*findSetterIt) : nullptr));

            m_properties.PushBack(propertyPtr);
            m_propertiesByName.Set(propertyPtr->GetName(), propertyPtr);

            continue;
        }

        HYP_FAIL("Invalid property definition for \"%s\": Must be HYP_FIELD() or getter/setter pair of HYP_METHOD()", it.first.Data());
    }
}

bool HypClass::CanSerialize() const
{
    if (m_serializationMode == HypClassSerializationMode::NONE)
    {
        return false;
    }

    if (m_serializationMode & HypClassSerializationMode::USE_MARSHAL_CLASS)
    {
        return true;
    }

    if (m_serializationMode & HypClassSerializationMode::MEMBERWISE)
    {
        return true;
    }

    if (m_serializationMode & HypClassSerializationMode::BITWISE)
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
    const auto it = m_propertiesByName.FindAs(name);

    if (it == m_propertiesByName.End())
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

        Array<HypProperty*> inheritedProperties = parent->GetPropertiesInherited();

        for (HypProperty* property : inheritedProperties)
        {
            properties.Insert(property);
        }

        return properties.ToArray();
    }

    return m_properties;
}

HypMethod* HypClass::GetMethod(WeakName name) const
{
    const auto it = m_methodsByName.FindAs(name);

    if (it == m_methodsByName.End())
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

        Array<HypMethod*> inheritedMethods = parent->GetMethodsInherited();

        for (HypMethod* method : inheritedMethods)
        {
            methods.Insert(method);
        }

        return methods.ToArray();
    }

    return m_methods;
}

HypField* HypClass::GetField(WeakName name) const
{
    const auto it = m_fieldsByName.FindAs(name);

    if (it == m_fieldsByName.End())
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

        Array<HypField*> inheritedFields = parent->GetFieldsInherited();

        for (HypField* field : inheritedFields)
        {
            fields.Insert(field);
        }

        return fields.ToArray();
    }

    return m_fields;
}

HypConstant* HypClass::GetConstant(WeakName name) const
{
    const auto it = m_constantsByName.FindAs(name);

    if (it == m_constantsByName.End())
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

        Array<HypConstant*> inheritedConstants = parent->GetConstantsInherited();

        for (HypConstant* constant : inheritedConstants)
        {
            constants.Insert(constant);
        }

        return constants.ToArray();
    }

    return m_constants;
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

    // fast path
    if (m_staticIndex >= 0)
    {
        return uint32(m_staticIndex - other->m_staticIndex) <= other->m_numDescendants;
    }

    // slow path
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

bool HypClass::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    if (!UseHandles())
    {
        return false;
    }

    const HypObjectBase* target = reinterpret_cast<const HypObjectBase*>(objectPtr);

    if (!target)
    {
        return false;
    }

    if (!target->GetManagedObjectResource())
    {
        return false;
    }

    TResourceHandle<ManagedObjectResource> resourceHandle(*target->GetManagedObjectResource());

    outObjectReference = resourceHandle->GetManagedObject()->GetObjectReference();
    
    return true;
}

#pragma endregion HypClass

} // namespace hyperion
