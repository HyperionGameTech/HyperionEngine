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

#ifdef HYP_DOTNET
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#endif

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

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

const HypClass* GetEnum(TypeId typeId)
{
    return HypClassRegistry::GetInstance().GetEnum(typeId);
}

const HypClass* GetEnum(WeakName typeName)
{
    return HypClassRegistry::GetInstance().GetEnum(typeName);
}

bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId)
{
    if (!ptr)
    {
        return false;
    }

    // we assume ptr is of the type TypeId, this is on the caller to ensure it's correct

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

        if (otherHypClass->UseHandles()) // check is HypObjectBase
        {
            // since we got the HypClass we can assume ptr is a HypObjectBase or derived type.
            // this could get iffy with multiple inheritance so it's best we disallow MI for HypObjects.
            const HypObjectBase* hypObjectBase = reinterpret_cast<const HypObjectBase*>(ptr);
            otherHypClass = hypObjectBase->InstanceClass();
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

HypProperty* MakeHypProperty(const HypField* field)
{
    HYP_CORE_ASSERT(field != nullptr);

    Name propertyName;

    if (const HypClassAttributeValue& attr = field->GetAttribute("property"); attr.IsString())
    {
        propertyName = CreateNameFromDynamicString(attr.GetString());
    }
    else
    {
        propertyName = field->GetName();
    }

    HypProperty* pResult = new HypProperty();
    HypProperty& result = *pResult;

    result.m_name = propertyName;
    result.m_typeId = field->GetTypeId();
    result.m_attributes = field->GetAttributes();

    result.m_getter = HypPropertyGetter();
    result.m_getter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_getter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_getter.getProc = [field](const HypData& target) -> HypData
    {
        return field->Get(target);
    };
    result.m_getter.serializeProc = [field](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
    {
        FBOMData data;

        if (!field->Serialize(target, data, flags))
        {
            return FBOMData();
        }

        return data;
    };

    result.m_setter = HypPropertySetter();
    result.m_setter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_setter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_setter.setProc = [field](HypData& target, const HypData& value) -> void
    {
        field->Set(target, value);
    };
    result.m_setter.deserializeProc = [field](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> void
    {
        field->Deserialize(context, target, value);
    };

    result.m_originalMember = field;

    return pResult;
}

HypProperty* MakeHypProperty(const HypMethod* getter, const HypMethod* setter)
{
    HypProperty* pResult = new HypProperty();
    HypProperty& result = *pResult;

    Optional<String> propertyAttributeOpt;

    Optional<TypeId> typeId;
    Optional<TypeId> targetTypeId;

    const bool hasGetter = getter != nullptr && getter->GetParameters().Size() >= 1;
    const bool hasSetter = setter != nullptr && setter->GetParameters().Size() >= 2;

    if (hasGetter)
    {
        if (const HypClassAttributeValue& attr = getter->GetAttribute("property"))
        {
            propertyAttributeOpt = attr.GetString();
        }

        typeId = getter->GetTypeId();
        targetTypeId = getter->GetParameters()[0].typeId;

        result.m_attributes = getter->GetAttributes();
    }

    if (hasSetter)
    {
        if (!propertyAttributeOpt)
        {
            if (const HypClassAttributeValue& attr = setter->GetAttribute("property"))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeId setterTypeId = setter->GetParameters()[0].typeId;

        if (typeId.HasValue())
        {
            HYP_CORE_ASSERT(*typeId == setterTypeId, "Getter TypeId (%u) does not match setter TypeId (%u)", typeId->Value(), setterTypeId.Value());
        }
        else
        {
            typeId = setterTypeId;
        }

        if (!typeId.HasValue())
        {
            typeId = setterTypeId;
        }

        if (targetTypeId.HasValue())
        {
            HYP_CORE_ASSERT(*targetTypeId == setter->GetTargetTypeId(), "Getter target TypeId (%u) does not match setter target TypeId (%u)", targetTypeId->Value(), setter->GetTargetTypeId().Value());
        }
        else
        {
            targetTypeId = setter->GetTargetTypeId();
        }

        result.m_attributes.Merge(setter->GetAttributes());
    }

    HYP_CORE_ASSERT(propertyAttributeOpt.HasValue(), "A HypProperty composed of getter/setter pair must have at least one method that has \"Property=\" attribute");
    HYP_CORE_ASSERT(typeId.HasValue(), "Cannot determine TypeId from getter/setter pair");

    result.m_name = CreateNameFromDynamicString(*propertyAttributeOpt);
    result.m_typeId = *typeId;

    if (hasGetter)
    {
        result.m_getter = HypPropertyGetter();
        result.m_getter.typeInfo.targetTypeId = *targetTypeId;
        result.m_getter.typeInfo.valueTypeId = *typeId;
        result.m_getter.getProc = [getter](const HypData& target) -> HypData
        {
            return getter->Invoke(Span<HypData> { const_cast<HypData*>(&target), 1 });
        };
        result.m_getter.serializeProc = [getter](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
        {
            FBOMData data;

            const bool result = getter->Serialize(Span<HypData> { const_cast<HypData*>(&target), 1 }, data, flags);

            HYP_CORE_ASSERT(result);

            return data;
        };
        result.m_originalMember = getter;
    }

    if (hasSetter)
    {
        result.m_setter = HypPropertySetter();
        result.m_setter.typeInfo.targetTypeId = *targetTypeId;
        result.m_setter.typeInfo.valueTypeId = setter->GetParameters()[0].typeId;
        result.m_setter.setProc = [setter](HypData& target, const HypData& value) -> void
        {
            setter->Invoke(Span<HypData*> { { &target, const_cast<HypData*>(&value) } });
        };
        result.m_setter.deserializeProc = [setter](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> void
        {
            const bool result = setter->Deserialize(context, target, value);

            HYP_CORE_ASSERT(result);
        };
        result.m_originalMember = setter;
    }

    return pResult;
}

using FormattedStringMap = HashMap<TypeId, String, HashTable_DynamicNodeAllocator<KeyValuePair<TypeId, String>>>;
thread_local FormattedStringMap* g_formattedStringMap;

static void InitFormattedStringMap(void* mem)
{
    Assert(mem != nullptr);
    FormattedStringMap& map = *new (mem) FormattedStringMap();

#define ADD_TYPE_NAME(type) map[TypeId::ForType<type>()] = #type

    ADD_TYPE_NAME(void);
    ADD_TYPE_NAME(bool);
    ADD_TYPE_NAME(int8);
    ADD_TYPE_NAME(uint8);
    ADD_TYPE_NAME(int16);
    ADD_TYPE_NAME(uint16);
    ADD_TYPE_NAME(int32);
    ADD_TYPE_NAME(uint32);
    ADD_TYPE_NAME(int64);
    ADD_TYPE_NAME(uint64);
    ADD_TYPE_NAME(float);
    ADD_TYPE_NAME(double);
    ADD_TYPE_NAME(char);
    ADD_TYPE_NAME(wchar_t);
    ADD_TYPE_NAME(size_t);
    ADD_TYPE_NAME(String);
    ADD_TYPE_NAME(ANSIString);
    ADD_TYPE_NAME(UTF16String);
    ADD_TYPE_NAME(UTF32String);
    ADD_TYPE_NAME(WideString);
    ADD_TYPE_NAME(Name);
    ADD_TYPE_NAME(WeakName);
    ADD_TYPE_NAME(TypeId);
    ADD_TYPE_NAME(HashCode);
    ADD_TYPE_NAME(void*);
    ADD_TYPE_NAME(char*);
    ADD_TYPE_NAME(const char*);
    ADD_TYPE_NAME(HypData);
    ADD_TYPE_NAME(AnyRef);
    ADD_TYPE_NAME(Any);

#undef ADD_TYPE_NAME
}

const char* LookupTypeName(TypeId typeId)
{
    const HypClass* hypClass = GetClass(typeId);

    if (hypClass)
    {
        return *hypClass->GetName();
    }

    if (!g_formattedStringMap)
    {
        ThreadBase* currentThreadObject = Threads::CurrentThreadObject();

        if (currentThreadObject == nullptr)
        {
            return "<could not lookup type name>";
        }

        g_formattedStringMap = currentThreadObject->GetTLS().Alloc<FormattedStringMap>();
        InitFormattedStringMap(g_formattedStringMap);

        currentThreadObject->AtExit([]()
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
      m_serializationMode(HypClassSerializationMode::DEFAULT),
      m_objectContainer(nullptr)
{
    static const HashMap<Name, HypClassFlags> s_attributeToFlags = {
        { NAME("abstract"), HypClassFlags::ABSTRACT },
        { NAME("noscriptbindings"), HypClassFlags::NO_SCRIPT_BINDINGS }
    };

    if (staticIndex >= 0)
    {
        HYP_CORE_ASSERT(staticIndex < g_maxStaticClassIndex, "Static index %d exceeds maximum static class index %u", staticIndex, g_maxStaticClassIndex);
    }

    // Apply flags for all values in s_attributeToFlags
    for (const HypClassAttribute& attr : m_attributes)
    {
        if (!attr.GetValue().GetBool())
        {
            // dont set flag if bool value is false
            continue;
        }

        auto it = s_attributeToFlags.Find(attr.name);

        if (it != s_attributeToFlags.End())
        {
            m_flags |= it->second;
        }
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
    HYP_LOG(Object, Info, "Initializing HypClass \"{}\"", m_name);

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

        if (!IsDynamic())
        {
            HYP_CORE_ASSERT(!m_parent->IsDynamic(), "Non-dynamic HypClass cannot have a dynamic parent class!");
        }
    }

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
            HypProperty* propertyPtr = MakeHypProperty(static_cast<HypField*>(*findFieldIt));

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
            HypProperty* propertyPtr = MakeHypProperty(
                findGetterIt != it.second.End() ? static_cast<HypMethod*>(*findGetterIt) : nullptr,
                findSetterIt != it.second.End() ? static_cast<HypMethod*>(*findSetterIt) : nullptr);

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

void HypClass::AddProperty(HypProperty* property)
{
    HYP_CORE_ASSERT(property != nullptr, "Cannot add null property to HypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(m_propertiesByName.Find(property->GetName()) == m_propertiesByName.End(),
        "Property with name %s already exists in HypClass %s", property->GetName().LookupString(), GetName().LookupString());

    m_properties.PushBack(property);
    m_propertiesByName[property->GetName()] = property;
}

void HypClass::AddMethod(HypMethod* method)
{
    HYP_CORE_ASSERT(method != nullptr, "Cannot add null method to HypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(m_methodsByName.Find(method->GetName()) == m_methodsByName.End(),
        "Method with name %s already exists in HypClass %s", method->GetName().LookupString(), GetName().LookupString());

    m_methods.PushBack(method);
    m_methodsByName[method->GetName()] = method;
}

void HypClass::AddField(HypField* field)
{
    HYP_CORE_ASSERT(field != nullptr, "Cannot add null field to HypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(m_fieldsByName.Find(field->GetName()) == m_fieldsByName.End(),
        "Field with name %s already exists in HypClass %s", field->GetName().LookupString(), GetName().LookupString());

    m_fields.PushBack(field);
    m_fieldsByName[field->GetName()] = field;
}

void HypClass::AddConstant(HypConstant* constant)
{
    HYP_CORE_ASSERT(constant != nullptr, "Cannot add null constant to HypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(m_constantsByName.Find(constant->GetName()) == m_constantsByName.End(),
        "Constant with name %s already exists in HypClass %s", constant->GetName().LookupString(), GetName().LookupString());

    m_constants.PushBack(constant);
    m_constantsByName[constant->GetName()] = constant;
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

#ifdef HYP_DOTNET

bool HypClass::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    if (!UseHandles()) // check is HypObjectBase
    {
        return false;
    }

    const HypObjectBase* target = reinterpret_cast<const HypObjectBase*>(objectPtr);

    if (!target)
    {
        return false;
    }

    if (!target->GetScriptObjectResource())
    {
        return false;
    }

    TResourceHandle<ScriptObjectResource> resourceHandle(*target->GetScriptObjectResource());

    outObjectReference = resourceHandle->GetManagedObject()->GetObjectReference();

    return true;
}

#endif

#pragma endregion HypClass

#pragma region DynamicHypClassInstance

#ifdef HYP_DOTNET
DynamicHypClassInstance::DynamicHypClassInstance(TypeId typeId, Name name, const HypClass* parentClass, dotnet::Class* classPtr, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : HypClass(typeId, name, -1, 0, parentClass ? parentClass->GetName() : Name::Invalid(), attributes, flags | HypClassFlags::DYNAMIC, members)
{
    m_refCount = 0;

    m_objectContainer = nullptr;

    if (classPtr != nullptr)
    {
        SetManagedClass(classPtr->RefCountedPtrFromThis());
    }

    m_parent = parentClass;
    m_parentName = parentClass->GetName();

    if (m_parent)
    {
        m_size = m_parent->GetSize();
        m_alignment = m_parent->GetAlignment();
        m_objectContainer = m_parent->GetObjectContainer();
    }
}
#endif

#ifdef HYP_SCRIPT
DynamicHypClassInstance::DynamicHypClassInstance(
    TypeId typeId,
    Name name,
    const HypClass* parentClass,
    Span<const HypClassAttribute> attributes,
    EnumFlags<HypClassFlags> flags,
    Span<HypMember> members)
    : HypClass(typeId, name, -1, 0, parentClass ? parentClass->GetName() : Name::Invalid(), attributes, flags | HypClassFlags::DYNAMIC, members)
{
    m_refCount = 0;

    m_parent = parentClass;

    SizeType dynamicSize = sizeof(HypObjectBase);
    SizeType dynamicAlignment = alignof(HypObjectBase);

    auto calculateDynamicHypClassSize = [](const HypClass* hypClass, SizeType& dynamicSize, SizeType& dynamicAlignment)
    {
        HYP_CORE_ASSERT(hypClass->IsDynamic());

        for (const HypField* field : hypClass->GetFields())
        {
            // In dynamic classes for scripts, all fields are stored as HypData
            const SizeType fieldSize = sizeof(HypData);
            const SizeType fieldAlignment = alignof(HypData);

            dynamicSize = ByteUtil::AlignAs(dynamicSize, fieldAlignment);

            HYP_CORE_ASSERT(field != nullptr);
            HYP_CORE_ASSERT(field->GetOffset() == dynamicSize, "Field offsets don't match expected offset! (field: %s, class: %s), expected %llu, got %llu",
                field->GetName().LookupString(), hypClass->GetName().LookupString(),
                dynamicSize, field->GetOffset());

            dynamicSize += fieldSize;

            dynamicAlignment = MathUtil::Max(dynamicAlignment, fieldAlignment);
        }
    };

    const HypClass* currentParent = m_parent;
    Array<const HypClass*> dynamicParents;

    while (currentParent != nullptr && currentParent->IsDynamic())
    {
        dynamicParents.PushBack(currentParent);

        currentParent = currentParent->GetParent();
    }

    // add size of first non-dynamic parent class (ensuring proper alignment)
    if (currentParent && !currentParent->IsDynamic())
    {
        dynamicSize = ByteUtil::AlignAs(dynamicSize, currentParent->GetAlignment());
        dynamicSize += currentParent->GetSize();

        dynamicAlignment = MathUtil::Max(dynamicAlignment, currentParent->GetAlignment());
    }

    // add 'class' field space
    dynamicSize = ByteUtil::AlignAs(dynamicSize, alignof(HypClassRef));
    dynamicSize += sizeof(HypClassRef);

    for (SizeType i = dynamicParents.Size(); i > 0; --i)
    {
        calculateDynamicHypClassSize(dynamicParents[i - 1], dynamicSize, dynamicAlignment);
    }

    calculateDynamicHypClassSize(this, dynamicSize, dynamicAlignment);

    // if no fields, we must at least be the size of HypObjectBase
    m_size = MathUtil::Max(sizeof(HypObjectBase), dynamicSize);
    m_alignment = MathUtil::Max(alignof(HypObjectBase), dynamicAlignment);

    m_objectContainer = &HypObjectPool::GetObjectContainerMap().GetOrCreate(m_typeId, this, [](const HypClass* thisHypClass) -> HypObjectContainerBase*
        {
            return new HypObjectContainer<HypObjectBase>(thisHypClass);
        });
}
#endif

DynamicHypClassInstance::~DynamicHypClassInstance()
{
    Assert(AtomicAdd(&m_refCount, 0) <= 0, "DynamicHypClassInstance destroyed while still being referenced!");
}

bool DynamicHypClassInstance::IsValid() const
{
    if (m_parent != nullptr)
    {
        return m_parent->IsValid();
    }

    return true;
}

HypClassAllocationMethod DynamicHypClassInstance::GetAllocationMethod() const
{
    if (m_parent != nullptr)
    {
        return m_parent->GetAllocationMethod();
    }

    return HypClassAllocationMethod::HANDLE;
}

TypeId DynamicHypClassInstance::GetUnderlyingTypeId() const
{
    if (!IsEnumType())
    {
        return HypClass::GetUnderlyingTypeId();
    }

    return m_enumUnderlyingTypeId;
}

#ifdef HYP_DOTNET
bool DynamicHypClassInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    Assert(m_parent != nullptr);
    Assert(m_parent->UseHandles(), "Must be HypObjectBase type to call GetManagedObject");

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(const_cast<void*>(objectPtr));
    Assert(target != nullptr);

    if (target->GetScriptObjectResource() == nullptr)
    {
        return false;
    }

    TResourceHandle<ScriptObjectResource> resourceHandle(*target->GetScriptObjectResource());

    if (!resourceHandle->GetManagedObject()->IsValid())
    {
        return false;
    }

    outObjectReference = resourceHandle->GetManagedObject()->GetObjectReference();

    return true;
}
#endif

bool DynamicHypClassInstance::CanCreateInstance() const
{
#ifdef HYP_DOTNET
    RC<dotnet::Class> managedClass = GetManagedClass();

    if (managedClass != nullptr)
    {
        Assert(m_parent != nullptr);

        return m_parent->CanCreateInstance()
            && !(managedClass->GetFlags() & ManagedClassFlags::ABSTRACT);
    }
#endif

#ifdef HYP_SCRIPT
    return true;
#endif

    return false;
}

bool DynamicHypClassInstance::ToHypData(ByteView memory, HypData& outHypData) const
{
    if (m_parent != nullptr)
    {
        return m_parent->ToHypData(memory, outHypData);
    }

#ifdef HYP_SCRIPT
    HYP_NOT_IMPLEMENTED(); // not yet implemented for script
#endif

    return false;
}

void DynamicHypClassInstance::PostLoad_Internal(void* objectPtr) const
{
}

bool DynamicHypClassInstance::CreateInstance_Internal(HypData& out) const
{
#ifdef HYP_DOTNET
    RC<dotnet::Class> managedClass = GetManagedClass();

    if (managedClass != nullptr)
    {
        Assert(m_parent != nullptr);

        // suppress default managed object creation - we will create it ourselves
        GlobalContextScope scope(HypObjectInitializerContext { this, HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        {
            HypData value;

            if (!m_parent->CreateInstance(value, /* allowAbstract */ true))
            {
                return false;
            }

            Assert(value.IsValid());

            if (m_parent->UseHandles())
            {
                AnyHandle handle = std::move(value.Get<AnyHandle>());
                Assert(handle.IsValid());

                out = HypData(AnyHandle(this, handle.Get()));
            }
            else
            {
                out = std::move(value);
            }
        }

        AssertDebug(m_parent->UseHandles());

        HypObjectBase* target = reinterpret_cast<HypObjectBase*>(out.ToRef().GetPointer());
        Assert(target != nullptr);

        ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>(HypObjectPtr(this, target), managedClass);
        AssertDebug(scriptObjectResource != nullptr);

        // keep it alive
        scriptObjectResource->IncRef();

        target->SetScriptObjectResource(scriptObjectResource);

        return true;
    }
#endif

#ifdef HYP_SCRIPT
    if (IsEnumType())
    {
        HYP_NOT_IMPLEMENTED(); // enum instance creation not yet implemented for scripts
    }

    Script_Value obj; // @TODO

    // get or create new container for dynamic type
    HypObjectContainer<HypObjectBase>* container = static_cast<HypObjectContainer<HypObjectBase>*>(GetObjectContainer());
    Assert(container != nullptr);
    Assert(container->GetObjectTypeId() == m_typeId);

    Array<const HypClass*> dynamicParents;
    const HypClass* topParent = m_parent;

    if (m_parent != nullptr)
    {
        while (topParent != nullptr)
        {
            if (!topParent->IsDynamic())
            {
                // stop after first non-dynamic parent class
                HYP_FAIL("Non-dynamic parent class construction not yet implemented for dynamic class {}, Parent class: {}",
                    GetName().LookupString(), topParent->GetName().LookupString());

                HYP_NOT_IMPLEMENTED(); // non-dynamic parent class construction not yet implemented

                break;
            }

            dynamicParents.PushBack(topParent);

            topParent = topParent->GetParent();
        }
    }

    PushGlobalContext(HypObjectInitializerContext { .hypClass = this, .flags = HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

    HypObjectHeader* header = container->Allocate(m_size, m_alignment);
    header->hypClass = this;

    HypObjectBase* ptr = HypObjectHeader::GetObjectPointer(header);
    new (ptr) HypObjectBase();

    // where to start writing fields
    SizeType fieldOffset = (topParent != nullptr && !topParent->IsDynamic() ? topParent->GetSize() : 0)
        + sizeof(HypObjectBase);

    // add 'class' field
    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypClassRef));
    HYP_CORE_ASSERT(fieldOffset + sizeof(HypClassRef) <= m_size,
        "Field offset out of bounds: %zu + %zu > %zu", fieldOffset, sizeof(HypClassRef), m_size);

    HypClassRef* classFieldPtr = (HypClassRef*)(UIntPtr(ptr) + fieldOffset);
    new (classFieldPtr) HypClassRef(this);
    fieldOffset += sizeof(HypClassRef);

    for (SizeType i = dynamicParents.Size(); i > 0; i--)
    {
        const HypClass* dynamicParent = dynamicParents[i - 1];
        HYP_CORE_ASSERT(dynamicParent->IsDynamic(), "Expected dynamic parent class");

        const DynamicHypClassInstance* dynamicParentInstance = static_cast<const DynamicHypClassInstance*>(dynamicParent);

        // Init all fields to HypData()
        for (HypField* field : dynamicParentInstance->GetFields())
        {
            // align field offset
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));

            HYP_CORE_ASSERT(fieldOffset + sizeof(HypData) <= m_size,
                "Field offset out of bounds: %zu + %zu > %zu", fieldOffset, sizeof(HypData), m_size);

            HypData* fieldPtr = (HypData*)(UIntPtr(ptr) + fieldOffset);
            new (fieldPtr) HypData();

            fieldOffset += sizeof(HypData);
        }
    }

    // our own class's fields lastly
    for (HypField* field : GetFields())
    {
        // align field offset
        fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));
        HYP_CORE_ASSERT(fieldOffset + sizeof(HypData) <= m_size,
            "Field offset out of bounds: %zu + %zu > %zu", fieldOffset, sizeof(HypData), m_size);

        HypData* fieldPtr = (HypData*)(UIntPtr(ptr) + fieldOffset);
        new (fieldPtr) HypData();

        fieldOffset += sizeof(HypData);
    }

    ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>((Script_Instance*)nullptr, std::move(obj), HYP_SCRIPT_TAG);
    Assert(scriptObjectResource != nullptr);
    ptr->SetScriptObjectResource(scriptObjectResource);

    Handle<HypObjectBase> handle;
    handle.ptr = static_cast<HypObjectBase*>(ptr);

    out = HypData(std::move(handle));

    PopGlobalContext<HypObjectInitializerContext>();

    return true;
#endif

    return false;
}

bool DynamicHypClassInstance::CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const
{
    HYP_NOT_IMPLEMENTED();
}

void DynamicHypClassInstance::SetField(uint32 index, HypField* field)
{
    HYP_CORE_ASSERT(field != nullptr, "Cannot set null field to DynamicHypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(!m_fieldsByName.Contains(field->GetName()),
        "Field with name %s already exists in DynamicHypClass %s", field->GetName().LookupString(), GetName().LookupString());

    if (index >= m_fields.Size())
    {
        m_fields.Resize(index + 1);
    }

    m_fields[index] = field;
    m_fieldsByName[field->GetName()] = field;
}

void DynamicHypClassInstance::SetMethod(uint32 index, HypMethod* method)
{
    HYP_CORE_ASSERT(method != nullptr, "Cannot set null method to DynamicHypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(!m_methodsByName.Contains(method->GetName()),
        "Method with name %s already exists in DynamicHypClass %s", method->GetName().LookupString(), GetName().LookupString());

    if (index >= m_methods.Size())
    {
        m_methods.Resize(index + 1);
    }

    m_methods[index] = method;
    m_methodsByName[method->GetName()] = method;
}

void DynamicHypClassInstance::SetProperty(uint32 index, HypProperty* property)
{
    HYP_CORE_ASSERT(property != nullptr, "Cannot set null property to DynamicHypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(!m_propertiesByName.Contains(property->GetName()),
        "Property with name %s already exists in DynamicHypClass %s", property->GetName().LookupString(), GetName().LookupString());

    if (index >= m_properties.Size())
    {
        m_properties.Resize(index + 1);
    }

    m_properties[index] = property;
    m_propertiesByName[property->GetName()] = property;
}

void DynamicHypClassInstance::SetConstant(uint32 index, HypConstant* constant)
{
    HYP_CORE_ASSERT(constant != nullptr, "Cannot set null constant to DynamicHypClass %s", GetName().LookupString());
    HYP_CORE_ASSERT(!m_constantsByName.Contains(constant->GetName()),
        "Constant with name %s already exists in DynamicHypClass %s", constant->GetName().LookupString(), GetName().LookupString());

    if (index >= m_constants.Size())
    {
        m_constants.Resize(index + 1);
    }

    m_constants[index] = constant;
    m_constantsByName[constant->GetName()] = constant;
}

void DynamicHypClassInstance::AddRef()
{
    AtomicIncrement(&m_refCount);
}

void DynamicHypClassInstance::Release()
{
    if (AtomicDecrement(&m_refCount) <= 0)
    {
        if (!HypClassRegistry::GetInstance().UnregisterClass(this))
        {
            HYP_LOG(Object, Warning, "Failed to unregister dynamic HypClass \"{}\"", GetName());
        }

        delete this;
    }
}

#pragma endregion DynamicHypClassInstance

#pragma region HypClassRef

HypClassRef::HypClassRef(const HypClass* hypClass, int initialRefCount)
    : hypClass(hypClass)
{
    if (hypClass && hypClass->IsDynamic() && initialRefCount > 0)
    {
        const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->AddRef();
    }
}

HypClassRef::HypClassRef(const HypClassRef& other)
    : hypClass(other.hypClass)
{
    if (hypClass)
    {
        if (hypClass->IsDynamic())
        {
            const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->AddRef();
        }
    }
}

HypClassRef& HypClassRef::operator=(const HypClassRef& other)
{
    if (this == &other || hypClass == other.hypClass)
    {
        return *this;
    }

    if (hypClass && hypClass->IsDynamic())
    {
        const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->Release();
    }

    hypClass = other.hypClass;

    if (hypClass && hypClass->IsDynamic())
    {
        const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->AddRef();
    }

    return *this;
}

HypClassRef::HypClassRef(HypClassRef&& other) noexcept
    : hypClass(other.hypClass)
{
    other.hypClass = nullptr;
}

HypClassRef& HypClassRef::operator=(HypClassRef&& other) noexcept
{
    if (this == &other || hypClass == other.hypClass)
    {
        return *this;
    }

    if (hypClass && hypClass->IsDynamic())
    {
        const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->Release();
    }

    hypClass = other.hypClass;
    other.hypClass = nullptr;

    return *this;
}

HypClassRef::~HypClassRef()
{
    if (hypClass && hypClass->IsDynamic())
    {
        const_cast<DynamicHypClassInstance*>(static_cast<const DynamicHypClassInstance*>(hypClass))->Release();
    }
}

#pragma endregion HypClassRef

} // namespace hyperion
