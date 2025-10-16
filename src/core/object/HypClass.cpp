/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypEnum.hpp>
#include <core/object/HypMember.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/ThreadLocalStorage.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

#ifdef HYP_DOTNET
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#endif

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <scripting/ScriptObjectResource.hpp>

#endif

#include <core/containers/ArrayMap.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion {
namespace Attributes {

HYP_API const Name g_attrSerialize = NAME("serialize");
HYP_API const Name g_attrTransient = NAME("transient");
HYP_API const Name g_attrComponent = NAME("component");
HYP_API const Name g_attrSize = NAME("size");
HYP_API const Name g_attrNoScriptBindings = NAME("noscriptbindings");
HYP_API const Name g_attrCommand = NAME("command");
HYP_API const Name g_attrAbstract = NAME("abstract");
HYP_API const Name g_attrDescription = NAME("description");
HYP_API const Name g_attrCompressed = NAME("compressed");
HYP_API const Name g_attrEditor = NAME("editor");
HYP_API const Name g_attrProperty = NAME("property");

} // namespace Attributes

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

#if 0
HypProperty* MakeHypProperty(const HypField* field)
{
    HYP_CORE_ASSERT(field != nullptr);

    Name propertyName;

    if (const HypClassAttributeValue& attr = field->GetAttribute(Attributes::g_attrProperty); attr.IsString())
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
    result.m_ownerClass = field->GetOwnerClass();

    result.m_getter = HypPropertyGetter();
    result.m_getter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_getter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_getter.getProc = [field](const HypData& target) -> HypData
    {
        return field->Get(target);
    };
    result.m_getter.serializeProc = [field](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
    {
        return field->Serialize(Span<HypData> { const_cast<HypData*>(&target), 1 }, out, flags);
    };

    result.m_setter = HypPropertySetter();
    result.m_setter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_setter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_setter.setProc = [field](HypData& target, const HypData& value) -> void
    {
        field->Set(target, value);
    };
    result.m_setter.deserializeProc = [field](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> Result
    {
        return field->Deserialize(context, target, value);
    };

    result.m_originalMember = field;

    return pResult;
}
#endif

HypProperty* MakeHypProperty(const HypField* field, const HypMethod* getter, const HypMethod* setter)
{
    HypProperty* pResult = new HypProperty();
    HypProperty& result = *pResult;

    Optional<String> propertyAttributeOpt;

    const TypeInfo* typeInfo = nullptr;
    const TypeInfo* targetTypeInfo = nullptr;

    const bool hasGetter = getter != nullptr && getter->GetParameters().Size() >= 1;
    const bool hasSetter = setter != nullptr && setter->GetParameters().Size() >= 2;

    if (hasGetter)
    {
        if (const HypClassAttributeValue& attr = getter->GetAttribute(Attributes::g_attrProperty))
        {
            propertyAttributeOpt = attr.GetString();
        }

        typeInfo = &getter->GetTypeInfo();
        targetTypeInfo = getter->GetParameters()[0].typeInfo;

        result.m_attributes = getter->GetAttributes();
    }

    if (field != nullptr)
    {
        if (!propertyAttributeOpt)
        {
            if (const HypClassAttributeValue& attr = field->GetAttribute(Attributes::g_attrProperty))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeInfo& fieldTypeInfo = field->GetTypeInfo();

        if (typeInfo != nullptr)
        {
            HYP_CORE_ASSERT(*typeInfo == fieldTypeInfo, "Getter type (%s) does not match field type (%s)", *typeInfo->name, *fieldTypeInfo.name);
        }
        else
        {
            typeInfo = &fieldTypeInfo;
        }

        if (targetTypeInfo != nullptr)
        {
            HYP_CORE_ASSERT(*targetTypeInfo == field->GetTargetTypeInfo(), "Getter target type (%s) does not match field target type (%s)", *targetTypeInfo->name, *field->GetTargetTypeInfo().name);
        }
        else
        {
            targetTypeInfo = &field->GetTargetTypeInfo();
        }
        result.m_attributes.Merge(field->GetAttributes());
    }

    if (hasSetter)
    {
        if (!propertyAttributeOpt)
        {
            if (const HypClassAttributeValue& attr = setter->GetAttribute(Attributes::g_attrProperty))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeInfo& setterTypeInfo = *setter->GetParameters()[0].typeInfo;

        if (typeInfo != nullptr)
        {
            HYP_CORE_ASSERT(*typeInfo == setterTypeInfo, "Getter/field type (%s) does not match setter type (%s)", *typeInfo->name, *setterTypeInfo.name);
        }
        else
        {
            typeInfo = &setterTypeInfo;
        }

        if (targetTypeInfo != nullptr)
        {
            HYP_CORE_ASSERT(*targetTypeInfo == setter->GetTargetTypeInfo(), "Getter/field target type (%s) does not match setter target type (%s)", *targetTypeInfo->name, *setter->GetTargetTypeInfo().name);
        }
        else
        {
            targetTypeInfo = &setter->GetTargetTypeInfo();
        }

        result.m_attributes.Merge(setter->GetAttributes());
    }

    HYP_CORE_ASSERT(propertyAttributeOpt.HasValue());
    HYP_CORE_ASSERT(typeInfo != nullptr, "Cannot determine TypeId from getter/setter pair or field");

    result.m_name = CreateNameFromDynamicString(*propertyAttributeOpt);
    result.m_typeInfo = typeInfo;
    result.m_ownerClass = nullptr;

    if (hasGetter)
    {
        result.m_getter = HypPropertyGetter();
        result.m_getter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_getter.typeInfo.valueTypeInfo = typeInfo;
        result.m_getter.getProc = [getter](const HypData& target) -> HypData
        {
            return getter->Invoke(Span<HypData> { const_cast<HypData*>(&target), 1 });
        };
        result.m_getter.serializeProc = [getter](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
        {
            return getter->Serialize(Span<HypData> { const_cast<HypData*>(&target), 1 }, out, flags);
        };
        result.m_originalMember = getter;
        result.m_ownerClass = getter->GetOwnerClass();
    }
    else if (field != nullptr)
    {
        result.m_getter = HypPropertyGetter();
        result.m_getter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_getter.typeInfo.valueTypeInfo = typeInfo;
        result.m_getter.getProc = [field](const HypData& target) -> HypData
        {
            return field->Get(target);
        };
        result.m_getter.serializeProc = [field](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
        {
            return field->Serialize(Span<HypData> { const_cast<HypData*>(&target), 1 }, out, flags);
        };

        if (!result.m_originalMember)
        {
            result.m_originalMember = field;
        }

        if (!result.m_ownerClass)
        {
            result.m_ownerClass = field->GetOwnerClass();
        }
    }

    if (hasSetter)
    {
        result.m_setter = HypPropertySetter();
        result.m_setter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_setter.typeInfo.valueTypeInfo = setter->GetParameters()[0].typeInfo;
        result.m_setter.setProc = [setter](HypData& target, const HypData& value) -> void
        {
            setter->Invoke(Span<HypData*> { { &target, const_cast<HypData*>(&value) } });
        };
        result.m_setter.deserializeProc = [setter](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> Result
        {
            return setter->Deserialize(context, target, value);
        };

        if (!result.m_originalMember)
        {
            result.m_originalMember = setter;
        }

        if (!result.m_ownerClass)
        {
            result.m_ownerClass = setter->GetOwnerClass();
        }
    }
    else if (field != nullptr)
    {
        result.m_setter = HypPropertySetter();
        result.m_setter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_setter.typeInfo.valueTypeInfo = typeInfo;
        result.m_setter.setProc = [field](HypData& target, const HypData& value) -> void
        {
            field->Set(target, value);
        };
        result.m_setter.deserializeProc = [field](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> Result
        {
            return field->Deserialize(context, target, value);
        };

        if (!result.m_originalMember)
        {
            result.m_originalMember = field;
        }

        if (!result.m_ownerClass)
        {
            result.m_ownerClass = field->GetOwnerClass();
        }
    }

    return pResult;
}

using FormattedStringMap = HashMap<TypeId, String, DynamicNodeAllocator>;
thread_local FormattedStringMap* g_formattedStringMap;

static void InitFormattedStringMap(void* mem)
{
    Assert(mem != nullptr);
    FormattedStringMap& map = *new (mem) FormattedStringMap();

#define ADD_TYPE_NAME(type) map[TypeId::ForType<type>()] = #type

    // pre-initialize some common type names for easier debugging
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
    ADD_TYPE_NAME(ConstAnyRef);
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

HypClassMemberIterator::HypClassMemberIterator(const HypClass* hypClass, EnumFlags<HypMemberType> memberTypes, Phase phase, bool deep)
    : m_memberTypes(memberTypes),
      m_phase(phase),
      m_deep(deep),
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
        m_target = m_deep ? m_target->GetParent() : nullptr;
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
      m_typeInfo(nullptr),
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
    // needs to be set after name is set
    m_typeInfo = &TypeInfo::ForHypClass(this);
    HYP_CORE_ASSERT(m_typeInfo != nullptr);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    // default to CORE pool
    m_enginePoolName = (EnginePoolName)0;
#endif

    // @NOTE: Can't reliably use the Attributes namespace values, as they might noe be
    // initialized yet by the time this constructor is called (static init order fiasco)
    static const ArrayMap<WeakName, HypClassFlags> s_attributeToFlags = {
        { "abstract", HypClassFlags::ABSTRACT },
        { "noscriptbindings", HypClassFlags::NO_SCRIPT_BINDINGS }
    };

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
        if (member.value.Is<HypProperty>())
        {
            HypProperty* pProperty = new HypProperty(std::move(member.value.GetUnchecked<HypProperty>()));
            pProperty->m_ownerClass = this;
            pProperty->m_getter.typeInfo.targetTypeInfo = m_typeInfo;
            pProperty->m_setter.typeInfo.targetTypeInfo = m_typeInfo;

            m_properties.PushBack(pProperty);
            m_propertiesByName.Set(pProperty->GetName(), pProperty);
        }
        else if (member.value.Is<HypMethod>())
        {
            HypMethod* pMethod = new HypMethod(std::move(member.value.GetUnchecked<HypMethod>()));
            pMethod->m_ownerClass = this;

            m_methods.PushBack(pMethod);
            m_methodsByName.Set(pMethod->GetName(), pMethod);
        }
        else if (member.value.Is<HypField>())
        {
            HypField* pField = new HypField(std::move(member.value.GetUnchecked<HypField>()));
            pField->m_ownerClass = this;

            m_fields.PushBack(pField);
            m_fieldsByName.Set(pField->GetName(), pField);
        }
        else if (member.value.Is<HypConstant>())
        {
            HypConstant* pConstant = new HypConstant(std::move(member.value.GetUnchecked<HypConstant>()));
            pConstant->m_ownerClass = this;

            m_constants.PushBack(pConstant);
            m_constantsByName.Set(pConstant->GetName(), pConstant);
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

    HYP_CORE_ASSERT(m_typeInfo != nullptr);

    m_serializationMode = HypClassSerializationMode::DEFAULT;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (UseHandles())
    {
        if (const HypClassAttributeValue& poolAttribute = GetAttributeDeep("pool"))
        {
            if (poolAttribute.IsString())
            {
                const EnginePoolName poolName = EnumValue<EnginePoolName>(poolAttribute.GetString(), (EnginePoolName)-1);

                if (poolName != (EnginePoolName)-1)
                {
                    m_enginePoolName = poolName;
                }
                else
                {
                    HYP_FAIL("Unknown engine pool name: {}", poolAttribute.GetString());
                }
            }
            else
            {
                HYP_FAIL("Engine pool attribute must be a string");
            }
        }
    }
#endif

    if (const HypClassAttributeValue& serializeAttribute = GetAttribute(Attributes::g_attrSerialize))
    {
        if (serializeAttribute.IsString())
        {
            m_serializationMode = HypClassSerializationMode::NONE;

            const String stringValue = serializeAttribute.GetString().ToLower();

            if (stringValue == "bitwise")
            {
                if (!IsPodType())
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

    for (IHypMember& member : GetMembers(/* includeProperties */ false, /* deep */ false))
    {
        if (const HypClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrProperty))
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

        if (findFieldIt != it.second.End() || findGetterIt != it.second.End() || findSetterIt != it.second.End())
        {
            HypProperty* pProperty = MakeHypProperty(
                findFieldIt != it.second.End() ? static_cast<HypField*>(*findFieldIt) : nullptr,
                findGetterIt != it.second.End() ? static_cast<HypMethod*>(*findGetterIt) : nullptr,
                findSetterIt != it.second.End() ? static_cast<HypMethod*>(*findSetterIt) : nullptr);

            HYP_CORE_ASSERT(pProperty->m_ownerClass && pProperty->m_ownerClass->IsBaseOf(this));
            HYP_CORE_ASSERT(!GetProperty(pProperty->GetName(), /* deep */ false), "Property with name \"%s\" already exists in class \"%s\"", *pProperty->GetName(), *GetName());

            m_properties.PushBack(pProperty);
            m_propertiesByName.Set(pProperty->GetName(), pProperty);

            continue;
        }

        HYP_FAIL("Invalid property definition for \"{}\": Must be HYP_FIELD() or getter/setter pair of HYP_METHOD()", it.first.Data());
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

IHypMember* HypClass::GetMember(WeakName name, EnumFlags<HypMemberType> memberTypes, bool deep) const
{
    if (memberTypes & HypMemberType::TYPE_PROPERTY)
    {
        if (HypProperty* property = GetProperty(name, /* deep */ false))
        {
            return property;
        }
    }

    if (memberTypes & HypMemberType::TYPE_FIELD)
    {
        if (HypField* field = GetField(name, /* deep */ false))
        {
            return field;
        }
    }

    if (memberTypes & HypMemberType::TYPE_METHOD)
    {
        if (HypMethod* method = GetMethod(name, /* deep */ false))
        {
            return method;
        }
    }

    if (memberTypes & HypMemberType::TYPE_CONSTANT)
    {
        if (HypConstant* constant = GetConstant(name, /* deep */ false))
        {
            return constant;
        }
    }

    if (deep)
    {
        if (const HypClass* parent = GetParent())
        {
            return parent->GetMember(name, memberTypes, /* deep */ true);
        }
    }

    return nullptr;
}

HypProperty* HypClass::GetProperty(WeakName name, bool deep) const
{
    const auto it = m_propertiesByName.FindAs(name);

    if (it == m_propertiesByName.End())
    {
        if (deep)
        {
            if (const HypClass* parent = GetParent())
            {
                return parent->GetProperty(name);
            }
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

HypMethod* HypClass::GetMethod(WeakName name, bool deep) const
{
    const auto it = m_methodsByName.FindAs(name);

    if (it == m_methodsByName.End())
    {
        if (deep)
        {
            if (const HypClass* parent = GetParent())
            {
                return parent->GetMethod(name);
            }
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

HypField* HypClass::GetField(WeakName name, bool deep) const
{
    const auto it = m_fieldsByName.FindAs(name);

    if (it == m_fieldsByName.End())
    {
        if (deep)
        {
            if (const HypClass* parent = GetParent())
            {
                return parent->GetField(name);
            }
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

HypConstant* HypClass::GetConstant(WeakName name, bool deep) const
{
    const auto it = m_constantsByName.FindAs(name);

    if (it == m_constantsByName.End())
    {
        if (deep)
        {
            if (const HypClass* parent = GetParent())
            {
                return parent->GetConstant(name);
            }
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

bool HypClass::IsBaseOf(const HypClass* other) const
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
    if (other->m_staticIndex >= 0)
    {
        return uint32(other->m_staticIndex - m_staticIndex) <= m_numDescendants;
    }

    // slow path
    const HypClass* current = other;

    while (current != nullptr)
    {
        if (current->m_parent == this)
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

    HypData obj; // @TODO

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

    HypObjectHeader* header = container->Allocate(m_size);
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

    ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>((Script_Instance*)nullptr, std::move(obj));
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
