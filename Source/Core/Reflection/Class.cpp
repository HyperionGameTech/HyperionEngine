/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/MemberVariant.hpp>
#include <Core/Reflection/Object.hpp>
#include <Core/Reflection/StaticField.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

#ifdef HYP_DOTNET
#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/Assembly.hpp>
#endif

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <Scripting/ScriptObjectResource.hpp>

#endif

#include <Core/Containers/ArrayMap.hpp>

namespace Hyperion {

#ifdef HYP_SCRIPT
#endif

#ifdef HYP_TOOL
const Class* g_clsObjectBase = nullptr;
#else
CORE_API extern const Class* g_clsObjectBase;
#endif

namespace Attributes {

CORE_API const Name g_attrSerialize = NAME("serialize");
CORE_API const Name g_attrDeserialize = NAME("deserialize");
CORE_API const Name g_attrTransient = NAME("transient");
CORE_API const Name g_attrComponent = NAME("component");
CORE_API const Name g_attrSize = NAME("size");
CORE_API const Name g_attrNoScriptBindings = NAME("noscriptbindings");
CORE_API const Name g_attrOnlyLanguages = NAME("onlylanguages");
CORE_API const Name g_attrCommand = NAME("command");
CORE_API const Name g_attrAbstract = NAME("abstract");
CORE_API const Name g_attrCompressed = NAME("compressed");
CORE_API const Name g_attrProperty = NAME("property");
CORE_API const Name g_attrLoadOrder = NAME("loadorder");
CORE_API const Name g_attrJsonPath = NAME("jsonpath");
CORE_API const Name g_attrJsonIgnore = NAME("jsonignore");
CORE_API const Name g_attrScriptableDelegate = NAME("scriptabledelegate");
CORE_API const Name g_attrFollowAssetPath = NAME("followassetpath");
CORE_API const Name g_attrSaveAsReference = NAME("saveasreference");

CORE_API const Name g_attrEditor = NAME("editor");
CORE_API const Name g_attrEditorOnly = NAME("editoronly");
CORE_API const Name g_attrEditOrder = NAME("editorder");
CORE_API const Name g_attrEditEnabled = NAME("editenabled");
CORE_API const Name g_attrEditHide = NAME("edithide");
CORE_API const Name g_attrLabel = NAME("label");
CORE_API const Name g_attrDescription = NAME("description");
CORE_API const Name g_attrEditAction = NAME("editaction");
CORE_API const Name g_attrEditCondition = NAME("editcondition");

} // namespace Attributes

#pragma region Helpers

CORE_API const Class* GetClass(const TypeId& typeId)
{
    return ClassRegistry::GetInstance().GetClass(typeId);
}

CORE_API const Class* GetClass(StringHash typeName)
{
    return ClassRegistry::GetInstance().GetClass(typeName);
}

CORE_API const Class* GetEnum(const TypeId& typeId)
{
    return ClassRegistry::GetInstance().GetEnum(typeId);
}

CORE_API const Class* GetEnum(StringHash typeName)
{
    return ClassRegistry::GetInstance().GetEnum(typeName);
}

CORE_API bool IsA(const Class* cls, const void* ptr, const TypeId& typeId)
{
    if (!ptr)
    {
        return false;
    }

    // we assume ptr is of the type TypeId, this is on the caller to ensure it's correct

    if (!cls)
    {
        return false;
    }

    if (cls->GetTypeId() == typeId)
    {
        return true;
    }

    const Class* otherClass = GetClass(typeId);

    if (otherClass != nullptr)
    {
        // fast path
        if (otherClass->GetStaticIndex() >= 0)
        {
            return uint32(otherClass->GetStaticIndex() - cls->GetStaticIndex()) <= cls->GetNumDescendants();
        }

        if (otherClass->UseHandles()) // check is ObjectBase
        {
            // since we got the Class we can assume ptr is a ObjectBase or derived type.
            // this could get iffy with multiple inheritance so it's best we disallow MI for Objects.
            const ObjectBase* casted = reinterpret_cast<const ObjectBase*>(ptr);
            otherClass = casted->InstanceClass();
        }
    }

    // slow path
    while (otherClass != nullptr)
    {
        if (otherClass == cls)
        {
            return true;
        }

        otherClass = otherClass->GetParent();
    }

    return false;
}

CORE_API bool IsA(const Class* cls, const Class* instanceClass)
{
    if (!cls || !instanceClass)
    {
        return false;
    }

    // fast path
    if (instanceClass->GetStaticIndex() >= 0)
    {
        return uint32(instanceClass->GetStaticIndex() - cls->GetStaticIndex()) <= cls->GetNumDescendants();
    }

    // slow path
    do
    {
        if (instanceClass == cls)
        {
            return true;
        }

        instanceClass = instanceClass->GetParent();
    }
    while (instanceClass != nullptr);

    return false;
}

CORE_API int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId)
{
    const Class* base = GetClass(baseTypeId);
    if (!base)
    {
        return -2;
    }

    const Class* subclass = GetClass(subclassTypeId);

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

CORE_API size_t GetNumDescendants(TypeId typeId)
{
    const Class* base = GetClass(typeId);
    if (!base)
    {
        return 0;
    }

    return base->GetNumDescendants();
}

#if 0
Property* MakeProperty(const Field* field)
{
    AssertDebug(field != nullptr);

    Name propertyName;

    if (const ClassAttributeValue& attr = field->GetAttribute(Attributes::g_attrProperty); attr.IsString())
    {
        propertyName = CreateNameFromDynamicString(attr.GetString());
    }
    else
    {
        propertyName = field->GetName();
    }

    Property* pResult = new Property();
    Property& result = *pResult;

    result.m_name = propertyName;
    result.m_typeId = field->GetTypeId();
    result.m_attributes = field->GetAttributes();
    result.m_ownerClass = field->GetOwnerClass();

    result.m_getter = PropertyGetter();
    result.m_getter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_getter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_getter.getProc = [field](const BoxedValue& target) -> BoxedValue
    {
        return field->Get(target);
    };

    result.m_setter = PropertySetter();
    result.m_setter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_setter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_setter.setProc = [field](BoxedValue& target, const BoxedValue& value) -> void
    {
        field->Set(target, value);
    };

    result.m_originalMember = field;

    return pResult;
}
#endif

Property* MakeProperty(const Field* field, const Method* getter, const Method* setter)
{
    Property* pResult = new Property();
    Property& result = *pResult;

    Optional<String> propertyAttributeOpt;

    const TypeInfo* typeInfo = nullptr;
    const TypeInfo* targetTypeInfo = nullptr;

    const bool hasGetter = getter != nullptr && getter->GetParameters().Size() >= 1;
    const bool hasSetter = setter != nullptr && setter->GetParameters().Size() >= 2;

    if (hasGetter)
    {
        if (const ClassAttributeValue& attr = getter->GetAttribute(Attributes::g_attrProperty))
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
            if (const ClassAttributeValue& attr = field->GetAttribute(Attributes::g_attrProperty))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeInfo& fieldTypeInfo = field->GetTypeInfo();

        if (typeInfo != nullptr)
        {
            AssertDebug(*typeInfo == fieldTypeInfo, "Getter type {} does not match field type {}", *typeInfo->name, *fieldTypeInfo.name);
        }
        else
        {
            typeInfo = &fieldTypeInfo;
        }

        if (targetTypeInfo != nullptr)
        {
            AssertDebug(*targetTypeInfo == field->GetTargetTypeInfo(), "Getter target type {} does not match field target type {}", *targetTypeInfo->name, *field->GetTargetTypeInfo().name);
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
            if (const ClassAttributeValue& attr = setter->GetAttribute(Attributes::g_attrProperty))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeInfo& setterTypeInfo = *setter->GetParameters()[0].typeInfo;

        if (typeInfo != nullptr)
        {
            AssertDebug(*typeInfo == setterTypeInfo, "Getter/field type {} does not match setter type {}", *typeInfo->name, *setterTypeInfo.name);
        }
        else
        {
            typeInfo = &setterTypeInfo;
        }

        if (targetTypeInfo != nullptr)
        {
            AssertDebug(*targetTypeInfo == setter->GetTargetTypeInfo(), "Getter/field target type {} does not match setter target type {}", *targetTypeInfo->name, *setter->GetTargetTypeInfo().name);
        }
        else
        {
            targetTypeInfo = &setter->GetTargetTypeInfo();
        }

        result.m_attributes.Merge(setter->GetAttributes());
    }

    AssertDebug(propertyAttributeOpt.HasValue());
    AssertDebug(typeInfo != nullptr, "Cannot determine TypeId from getter/setter pair or field");

    result.m_name = CreateNameFromDynamicString(*propertyAttributeOpt);
    result.m_typeInfo = typeInfo;
    result.m_ownerClass = nullptr;

    if (hasGetter)
    {
        result.m_getter = PropertyGetter();
        result.m_getter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_getter.typeInfo.valueTypeInfo = typeInfo;
        result.m_getter.getProc = [getter](const BoxedValue& target) -> BoxedValue
        {
            return getter->Invoke(Span<BoxedValue> { const_cast<BoxedValue*>(&target), 1 });
        };

        result.m_originalMember = getter;
        result.m_ownerClass = getter->GetOwnerClass();
    }
    else if (field != nullptr)
    {
        result.m_getter = PropertyGetter();
        result.m_getter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_getter.typeInfo.valueTypeInfo = typeInfo;
        result.m_getter.getProc = [field](const BoxedValue& target) -> BoxedValue
        {
            return field->Get(target);
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
        result.m_setter = PropertySetter();
        result.m_setter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_setter.typeInfo.valueTypeInfo = setter->GetParameters()[0].typeInfo;
        result.m_setter.setProc = [setter](BoxedValue& target, const BoxedValue& value) -> void
        {
            setter->Invoke(Span<BoxedValue*> { { &target, const_cast<BoxedValue*>(&value) } });
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
        result.m_setter = PropertySetter();
        result.m_setter.typeInfo.targetTypeInfo = targetTypeInfo;
        result.m_setter.typeInfo.valueTypeInfo = typeInfo;
        result.m_setter.setProc = [field](BoxedValue& target, const BoxedValue& value) -> void
        {
            field->Set(target, value);
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

using FormattedStringMap = TMap<TypeId, String, DynamicAllocator, HashTablePolicy::NotPooled>;
thread_local FormattedStringMap* t_formattedStringMap;

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
    ADD_TYPE_NAME(StringHash);
    ADD_TYPE_NAME(TypeId);
    ADD_TYPE_NAME(HashCode);
    ADD_TYPE_NAME(void*);
    ADD_TYPE_NAME(char*);
    ADD_TYPE_NAME(const char*);
    ADD_TYPE_NAME(BoxedValue);
    ADD_TYPE_NAME(ConstAnyRef);
    ADD_TYPE_NAME(AnyRef);
    ADD_TYPE_NAME(Any);

#undef ADD_TYPE_NAME
}

const char* LookupTypeName(const TypeId& typeId)
{
    const Class* cls = GetClass(typeId);

    if (cls)
    {
        return *cls->GetName();
    }

    if (!t_formattedStringMap)
    {
        ThreadBase* currentThreadObject = CurrentThreadObject();

        if (currentThreadObject == nullptr)
        {
            return "<could not lookup type name>";
        }

        t_formattedStringMap = currentThreadObject->GetTLS().Allocate<FormattedStringMap>();
        InitFormattedStringMap(t_formattedStringMap);

        currentThreadObject->AddOnExitCallback([]()
                                               {
                                                   t_formattedStringMap->~FormattedStringMap();
                                               });
    }

    auto it = t_formattedStringMap->Find(typeId);

    if (it == t_formattedStringMap->End())
    {
        it = t_formattedStringMap->Insert(typeId, HYP_FORMAT("TypeId({})", typeId.Value())).first;
    }

    return *it->second;
}

#pragma endregion Helpers

#pragma region ClassMemberIterator

ClassMemberIterator::ClassMemberIterator(const Class* cls, EnumFlags<MemberType> memberTypes, Phase phase, bool deep)
    : m_memberTypes(memberTypes),
      m_phase(phase),
      m_deep(deep),
      m_target(cls),
      m_currentIndex(0),
      m_currentValue(nullptr)
{
    Advance();
}

void ClassMemberIterator::Advance()
{
    // HYP_LOG(Object, Verbose, "Iterating class {} members: {}, parent = {}, index = {}", target->GetName(), m_phase,
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
    case Phase::ITERATE_STATIC_FIELDS:
        if ((m_memberTypes & MemberType::StaticField) && m_currentIndex < m_target->GetStaticFields().Size())
        {
            m_currentValue = m_target->GetStaticFields()[m_currentIndex++];
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
        if ((m_memberTypes & MemberType::Property) && m_currentIndex < m_target->GetProperties().Size())
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
        if ((m_memberTypes & MemberType::Method) && m_currentIndex < m_target->GetMethods().Size())
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
        if ((m_memberTypes & MemberType::Field) && m_currentIndex < m_target->GetFields().Size())
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

#pragma endregion ClassMemberIterator

#pragma region Class

Class::Class(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<MemberVariant> members)
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
      m_serializationMode(ClassSerializationMode::DEFAULT),
      m_objectContainer(nullptr)
{
    // needs to be set after name is set
    m_typeInfo = (m_flags & ClassFlags::DYNAMIC) ? TypeInfo::ForDynamicClass(this) : &TypeInfo::ForClass(this);
    HYP_CORE_ASSERT(m_typeInfo != nullptr);

    // @NOTE: Can't reliably use the Attributes namespace values, as they might noe be
    // initialized yet by the time this constructor is called (static init order fiasco)
    static const ArrayMap<StringHash, ClassFlags> s_attributeToFlags = {
        { "abstract"_sh, ClassFlags::ABSTRACT },
        { "noscriptbindings"_sh, ClassFlags::NO_SCRIPT_BINDINGS }
    };

    // Apply flags for all values in s_attributeToFlags
    for (const ClassAttribute& attr : m_attributes)
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
    for (MemberVariant& member : members)
    {
        HYP_CORE_ASSERT(member.internal != nullptr);

        switch (member.internal->GetMemberType())
        {
        case MemberType::Property:
        {
            Property* property = static_cast<Property*>(member.internal);
            member.internal = nullptr;

            property->m_ownerClass = this;
            property->m_getter.typeInfo.targetTypeInfo = m_typeInfo;
            property->m_setter.typeInfo.targetTypeInfo = m_typeInfo;

            m_properties.PushBack(property);
            m_propertiesByName.Set(property->GetName(), property);

            break;
        }
        case MemberType::Method:
        {
            Method* method = static_cast<Method*>(member.internal);
            member.internal = nullptr;

            method->m_ownerClass = this;

            m_methods.PushBack(method);
            m_methodsByName.Set(method->GetName(), method);

            break;
        }
        case MemberType::Field:
        {
            Field* field = static_cast<Field*>(member.internal);
            member.internal = nullptr;

            field->m_ownerClass = this;

            m_fields.PushBack(field);
            m_fieldsByName.Set(field->GetName(), field);

            break;
        }
        case MemberType::StaticField:
        {
            StaticField* staticField = static_cast<StaticField*>(member.internal);
            member.internal = nullptr;

            staticField->m_ownerClass = this;

            m_staticFields.PushBack(staticField);
            m_staticFieldsByName.Set(staticField->GetName(), staticField);

            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }
}

Class::~Class()
{
    for (Property* propertyPtr : m_properties)
    {
        delete propertyPtr;
    }

    for (Method* methodPtr : m_methods)
    {
        delete methodPtr;
    }

    for (Field* fieldPtr : m_fields)
    {
        delete fieldPtr;
    }

    for (StaticField* constantPtr : m_staticFields)
    {
        delete constantPtr;
    }

    // for dynamic classes, we own the TypeInfo and need to delete it manually
    if (IsDynamic())
    {
        delete m_typeInfo;
        m_typeInfo = nullptr;
    }
}

void Class::Initialize()
{
    AssertDebug(m_typeInfo != nullptr);

    m_serializationMode = ClassSerializationMode::DEFAULT;

    if (const ClassAttributeValue& serializeAttribute = GetAttribute(Attributes::g_attrSerialize))
    {
        if (serializeAttribute.IsString())
        {
            m_serializationMode = ClassSerializationMode::NONE;

            String stringValue = serializeAttribute.GetString();
            stringValue = stringValue.ToLower();

            if (stringValue == "bitwise")
            {
                if (!IsPodType())
                {
                    HYP_FAIL("Cannot use \"bitwise\" serialization mode for non-POD type: {}", m_name);
                }

                m_serializationMode = ClassSerializationMode::BITWISE;
            }
            else
            {
                HYP_FAIL("Unknown serialization mode: {}", stringValue);
            }
        }
        else if (!serializeAttribute.GetBool())
        {
            m_serializationMode = ClassSerializationMode::NONE;
        }
    }

    if (m_parentName.IsValid())
    {
        if (!m_parent)
        {
            m_parent = GetClass(m_parentName);
        }

        printf("parent class for %s = %s\n",
               m_name.LookupString(),
               m_parentName.LookupString());

        HYP_CORE_ASSERT(m_parent != nullptr, "Invalid parent class: {}", m_parentName);

        if (!IsDynamic())
        {
            HYP_CORE_ASSERT(!m_parent->IsDynamic(), "Non-dynamic Class cannot have a dynamic parent class!");
        }
    }

    // Build properties from `Property=` attributes on methods and fields
    Array<Pair<String, Array<IMember*>>> propertiesToBuild;

    for (IMember& member : GetMembers(/* includeProperties */ false, /* deep */ false))
    {
        if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrProperty))
        {
            const String& attrString = attr.GetString();

            auto propertiesToBuildIt = propertiesToBuild.FindIf([&attrString](const auto& item)
                                                                {
                                                                    return item.first == attrString;
                                                                });

            if (propertiesToBuildIt == propertiesToBuild.End())
            {
                propertiesToBuildIt = &propertiesToBuild.EmplaceBack(attrString, Array<IMember*> {});
            }

            propertiesToBuildIt->second.PushBack(&member);
        }
    }

    for (const Pair<String, Array<IMember*>>& it : propertiesToBuild)
    {
        if (it.second.Empty())
        {
            continue;
        }

        const auto findFieldIt = it.second.FindIf([](IMember* member)
                                                  {
                                                      return member->GetMemberType() == MemberType::Field;
                                                  });

        const auto findGetterIt = it.second.FindIf([](IMember* member)
                                                   {
                                                       return member->GetMemberType() == MemberType::Method
                                                           && static_cast<Method*>(member)->GetParameters().Size() == 1;
                                                   });

        const auto findSetterIt = it.second.FindIf([](IMember* member)
                                                   {
                                                       return member->GetMemberType() == MemberType::Method
                                                           && static_cast<Method*>(member)->GetParameters().Size() == 2;
                                                   });

        if (findFieldIt != it.second.End() || findGetterIt != it.second.End() || findSetterIt != it.second.End())
        {
            Property* property = MakeProperty(
                findFieldIt != it.second.End() ? static_cast<Field*>(*findFieldIt) : nullptr,
                findGetterIt != it.second.End() ? static_cast<Method*>(*findGetterIt) : nullptr,
                findSetterIt != it.second.End() ? static_cast<Method*>(*findSetterIt) : nullptr);

            HYP_CORE_ASSERT(property->m_ownerClass && property->m_ownerClass->IsBaseOf(this));
            HYP_CORE_ASSERT(!GetProperty(property->GetName(), /* deep */ false), "Property with name \"{}\" already exists in class \"{}\"", *property->GetName(), *GetName());

            m_properties.PushBack(property);
            m_propertiesByName.Set(property->GetName(), property);

            continue;
        }

        HYP_FAIL("Invalid property definition for \"{}\": Must be HYP_FIELD() or getter/setter pair of HYP_METHOD()", it.first.Data());
    }
}

bool Class::CanSerialize() const
{
    if (m_serializationMode == ClassSerializationMode::NONE)
    {
        return false;
    }

    if (m_serializationMode & ClassSerializationMode::MEMBERWISE)
    {
        return true;
    }

    if (m_serializationMode & ClassSerializationMode::BITWISE)
    {
        if (IsStructType())
        {
            return true;
        }
    }

    return false;
}

IMember* Class::GetMember(StringHash name, EnumFlags<MemberType> memberTypes, bool deep) const
{
    if (memberTypes & MemberType::Property)
    {
        if (Property* property = GetProperty(name, /* deep */ false))
        {
            return property;
        }
    }

    if (memberTypes & MemberType::Field)
    {
        if (Field* field = GetField(name, /* deep */ false))
        {
            return field;
        }
    }

    if (memberTypes & MemberType::Method)
    {
        if (Method* method = GetMethod(name, /* deep */ false))
        {
            return method;
        }
    }

    if (memberTypes & MemberType::StaticField)
    {
        if (StaticField* staticField = GetStaticField(name, /* deep */ false))
        {
            return staticField;
        }
    }

    if (deep)
    {
        if (const Class* parent = GetParent())
        {
            return parent->GetMember(name, memberTypes, /* deep */ true);
        }
    }

    return nullptr;
}

Property* Class::GetProperty(StringHash name, bool deep) const
{
    const auto it = m_propertiesByName.FindAs(name);

    if (it == m_propertiesByName.End())
    {
        if (deep)
        {
            if (const Class* parent = GetParent())
            {
                return parent->GetProperty(name);
            }
        }

        return nullptr;
    }

    return it->second;
}

Array<Property*> Class::GetPropertiesInherited() const
{
    if (const Class* parent = GetParent())
    {
        TFlatSet<Property*> properties { GetProperties().Begin(), GetProperties().End() };

        Array<Property*> inheritedProperties = parent->GetPropertiesInherited();

        for (Property* property : inheritedProperties)
        {
            properties.Insert(property);
        }

        return properties.ToArray();
    }

    return m_properties;
}

Method* Class::GetMethod(StringHash name, bool deep) const
{
    const auto it = m_methodsByName.FindAs(name);

    if (it == m_methodsByName.End())
    {
        if (deep)
        {
            if (const Class* parent = GetParent())
            {
                return parent->GetMethod(name);
            }
        }

        return nullptr;
    }

    return it->second;
}

Array<Method*> Class::GetMethodsInherited() const
{
    if (const Class* parent = GetParent())
    {
        TFlatSet<Method*> methods { m_methods.Begin(), m_methods.End() };

        Array<Method*> inheritedMethods = parent->GetMethodsInherited();

        for (Method* method : inheritedMethods)
        {
            methods.Insert(method);
        }

        return methods.ToArray();
    }

    return m_methods;
}

Field* Class::GetField(StringHash name, bool deep) const
{
    const auto it = m_fieldsByName.FindAs(name);

    if (it == m_fieldsByName.End())
    {
        if (deep)
        {
            if (const Class* parent = GetParent())
            {
                return parent->GetField(name);
            }
        }

        return nullptr;
    }

    return it->second;
}

Array<Field*> Class::GetFieldsInherited() const
{
    if (const Class* parent = GetParent())
    {
        TFlatSet<Field*> fields { m_fields.Begin(), m_fields.End() };

        Array<Field*> inheritedFields = parent->GetFieldsInherited();

        for (Field* field : inheritedFields)
        {
            fields.Insert(field);
        }

        return fields.ToArray();
    }

    return m_fields;
}

StaticField* Class::GetStaticField(StringHash name, bool deep) const
{
    const auto it = m_staticFieldsByName.FindAs(name);

    if (it == m_staticFieldsByName.End())
    {
        if (deep)
        {
            if (const Class* parent = GetParent())
            {
                return parent->GetStaticField(name);
            }
        }

        return nullptr;
    }

    return it->second;
}

Array<StaticField*> Class::GetStaticFieldsInherited() const
{
    if (const Class* parent = GetParent())
    {
        TFlatSet<StaticField*> staticFields { m_staticFields.Begin(), m_staticFields.End() };

        Array<StaticField*> inheritedConstants = parent->GetStaticFieldsInherited();

        for (StaticField* staticField : inheritedConstants)
        {
            staticFields.Insert(staticField);
        }

        return staticFields.ToArray();
    }

    return m_staticFields;
}

void Class::AddProperty(Property* property)
{
    AssertDebug(property != nullptr, "Cannot add null property to Class {}", GetName());
    AssertDebug(m_propertiesByName.Find(property->GetName()) == m_propertiesByName.End(),
                "Property with name {} already exists in Class {}", property->GetName(), GetName());

    m_properties.PushBack(property);
    m_propertiesByName[property->GetName()] = property;
}

void Class::AddMethod(Method* method)
{
    AssertDebug(method != nullptr, "Cannot add null method to Class {}", GetName());
    AssertDebug(m_methodsByName.Find(method->GetName()) == m_methodsByName.End(),
                "Method with name {} already exists in Class {}", method->GetName(), GetName());

    m_methods.PushBack(method);
    m_methodsByName[method->GetName()] = method;
}

void Class::AddField(Field* field)
{
    AssertDebug(field != nullptr, "Cannot add null field to Class {}", GetName());
    AssertDebug(m_fieldsByName.Find(field->GetName()) == m_fieldsByName.End(),
                "Field with name {} already exists in Class {}", field->GetName(), GetName());

    m_fields.PushBack(field);
    m_fieldsByName[field->GetName()] = field;
}

void Class::AddStaticField(StaticField* staticField)
{
    AssertDebug(staticField != nullptr, "Cannot add null static field to Class {}", GetName());
    AssertDebug(m_staticFieldsByName.Find(staticField->GetName()) == m_staticFieldsByName.End(),
                "Static field with name {} already exists in Class {}", staticField->GetName(), GetName());

    m_staticFields.PushBack(staticField);
    m_staticFieldsByName[staticField->GetName()] = staticField;
}

bool Class::IsDerivedFrom(const Class* other) const
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
    const Class* current = this;

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

bool Class::IsBaseOf(const Class* other) const
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
    const Class* current = other;

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

bool Class::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    if (!UseHandles()) // check is ObjectBase
    {
        return false;
    }

    const ObjectBase* target = reinterpret_cast<const ObjectBase*>(objectPtr);

    if (!target)
    {
        return false;
    }

    ScriptObjectResource* sor = target->GetScriptObjectResource();

    if (!sor)
    {
        return false;
    }

    dotnet::ManagedObject* managedObject = ScriptObjectFunctions::GetManagedObject(sor);

    if (!managedObject)
    {
        return false;
    }

    outObjectReference = managedObject->GetObjectReference();

    return true;
}

#endif

#pragma endregion Class

#pragma region DynamicClassInstance

#ifdef HYP_DOTNET
DynamicClassInstance::DynamicClassInstance(TypeId typeId, Name name, const Class* parentClass, dotnet::ManagedClass* pManagedClass, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<MemberVariant> members)
    : Class(typeId, name, -1, 0, parentClass ? parentClass->GetName() : g_clsObjectBase->GetName(), attributes, flags | ClassFlags::DYNAMIC, members)
{
    m_refCount = 0;

    m_objectContainer = nullptr;

    if (pManagedClass != nullptr)
    {
        SetManagedClass(ScriptObjectFunctions::ManagedClassSharedThis(pManagedClass));
    }

    m_parent = parentClass != nullptr ? parentClass : g_clsObjectBase;

    if (m_parent)
    {
        m_size = m_parent->GetSize();
        m_alignment = m_parent->GetAlignment();
        m_objectContainer = m_parent->GetObjectContainer();
    }
}
#endif

#ifdef HYP_SCRIPT
DynamicClassInstance::DynamicClassInstance(
    TypeId typeId,
    Name name,
    const Class* parentClass,
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<MemberVariant> members)
    : Class(typeId, name, -1, 0, parentClass ? parentClass->GetName() : g_clsObjectBase->GetName(), attributes, flags | ClassFlags::DYNAMIC, members)
{
    m_refCount = 0;

    m_parent = parentClass != nullptr ? parentClass : g_clsObjectBase;

    size_t dynamicSize = sizeof(ObjectBase);
    size_t dynamicAlignment = alignof(ObjectBase);

    auto CalculateDynamicClassSize = [](const Class* cls, size_t& dynamicSize, size_t& dynamicAlignment)
    {
        AssertDebug(cls->IsDynamic());

        for (const Field* field : cls->GetFields())
        {
            // In dynamic classes for scripts, all fields are stored as BoxedValue
            const size_t fieldSize = sizeof(BoxedValue);
            const size_t fieldAlignment = alignof(BoxedValue);

            dynamicSize = ByteUtil::AlignAs(dynamicSize, fieldAlignment);

            AssertDebug(field != nullptr);
            AssertDebug(field->GetOffset() == dynamicSize, "Field offsets don't match expected offset! (field: {}, class: {}), expected {}, got {}",
                        field->GetName(), cls->GetName(),
                        dynamicSize, field->GetOffset());

            dynamicSize += fieldSize;

            dynamicAlignment = MathUtil::Max(dynamicAlignment, fieldAlignment);
        }
    };

    const Class* currentParent = m_parent;
    Array<const Class*> dynamicParents;

    while (currentParent != nullptr && currentParent->IsDynamic() && currentParent != g_clsObjectBase)
    {
        dynamicParents.PushBack(currentParent);

        currentParent = currentParent->GetParent();
    }

    // add size of first non-dynamic parent class (ensuring proper alignment)
    if (currentParent && !currentParent->IsDynamic() && currentParent != g_clsObjectBase)
    {
        dynamicSize = ByteUtil::AlignAs(dynamicSize, currentParent->GetAlignment());
        dynamicSize += currentParent->GetSize();

        dynamicAlignment = MathUtil::Max(dynamicAlignment, currentParent->GetAlignment());
    }

    for (size_t i = dynamicParents.Size(); i > 0; --i)
    {
        CalculateDynamicClassSize(dynamicParents[i - 1], dynamicSize, dynamicAlignment);
    }

    CalculateDynamicClassSize(this, dynamicSize, dynamicAlignment);

    // if no fields, we must at least be the size of ObjectBase
    m_size = MathUtil::Max(sizeof(ObjectBase), dynamicSize);
    m_alignment = MathUtil::Max(alignof(ObjectBase), dynamicAlignment);

    m_objectContainer = &GetObjectContainerMap().GetOrCreate(m_typeId, this, [](const Class* thisClass) -> ObjectContainerBase*
                                                             {
                                                                 ObjectContainer<ObjectBase>* container = new ObjectContainer<ObjectBase>(thisClass);

                                                                 // we use the Script pool for allocating instances of dynamic classes when HYP_SCRIPT is enabled
                                                                 Pool* scriptPool = ScriptObjectFunctions::GetScriptPool ? ScriptObjectFunctions::GetScriptPool() : nullptr;
                                                                 Assert(scriptPool != nullptr);
                                                                 container->SetPool(scriptPool);

                                                                 return container;
                                                             });

    Assert(m_objectContainer != nullptr);
}
#endif

DynamicClassInstance::~DynamicClassInstance()
{
    Assert(AtomicAdd(&m_refCount, 0) <= 0, "DynamicClassInstance destroyed while still being referenced!");
}

bool DynamicClassInstance::IsValid() const
{
    if (m_parent != nullptr)
    {
        return m_parent->IsValid();
    }

    return true;
}

ClassAllocationMethod DynamicClassInstance::GetAllocationMethod() const
{
    if (m_parent != nullptr)
    {
        return m_parent->GetAllocationMethod();
    }

    return ClassAllocationMethod::HANDLE;
}

TypeId DynamicClassInstance::GetUnderlyingTypeId() const
{
    if (!IsEnumType())
    {
        return Class::GetUnderlyingTypeId();
    }

    return m_enumUnderlyingTypeId;
}

#ifdef HYP_DOTNET
bool DynamicClassInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    Assert(m_parent != nullptr);
    Assert(m_parent->UseHandles(), "Must be ObjectBase type to call GetManagedObject");

    ObjectBase* target = reinterpret_cast<ObjectBase*>(const_cast<void*>(objectPtr));
    Assert(target != nullptr);

    if (target->GetScriptObjectResource() == nullptr)
    {
        return false;
    }

    ResourceGuard resourceScope = target->GetScriptObjectResource()->GetReadScope();

    dotnet::ManagedObject* managedObject = ScriptObjectFunctions::GetManagedObject(target->GetScriptObjectResource());

    if (!managedObject || !managedObject->IsValid())
    {
        return false;
    }

    outObjectReference = managedObject->GetObjectReference();

    return true;
}
#endif

bool DynamicClassInstance::CanCreateInstance() const
{
#ifdef HYP_DOTNET
    SharedPtr<dotnet::ManagedClass> managedClass = GetManagedClass();

    if (managedClass != nullptr)
    {
        Assert(m_parent != nullptr);

        if (m_parent->CanCreateInstance() && !(managedClass->GetFlags() & ManagedClassFlags::ABSTRACT))
        {
            return true;
        }
    }
#endif

#ifdef HYP_SCRIPT
    return true;
#endif

    return false;
}

bool DynamicClassInstance::ToBoxed(ByteView memory, BoxedValue& outBoxed) const
{
    if (m_parent != nullptr)
    {
        return m_parent->ToBoxed(memory, outBoxed);
    }

#ifdef HYP_SCRIPT
    HYP_NOT_IMPLEMENTED(); // not yet implemented for script
#endif

    return false;
}

void DynamicClassInstance::PostLoad_Internal(void* objectPtr) const
{
}

bool DynamicClassInstance::CreateInstance_Internal(BoxedValue& out) const
{
    ScriptObjectResource* scriptObjectResource = nullptr;
    bool isCreated = false;

#ifdef HYP_DOTNET
    SharedPtr<dotnet::ManagedClass> managedClass = GetManagedClass();

    if (managedClass != nullptr)
    {
        Assert(m_parent != nullptr);

        {
            // suppress default managed object creation - we will create it ourselves
            GlobalContextScope scope(ObjectInitializerContext { this, ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

            {
                BoxedValue value;

                if (!m_parent->CreateInstance(value, /* allowAbstract */ true))
                {
                    HYP_FAIL("Failed to create instance of parent class {} for dynamic class {}", m_parent->GetName(), GetName());
                }

                Assert(value.IsValid());

                if (m_parent->UseHandles())
                {
                    Handle<ObjectBase> handle = std::move(value.Get<Handle<ObjectBase>>());
                    Assert(handle.IsValid());

                    out = BoxedValue(std::move(handle));
                }
                else
                {
                    out = std::move(value);
                }
            }
        }

        AssertDebug(m_parent->UseHandles());

        ObjectBase* target = reinterpret_cast<ObjectBase*>(out.ToRef().GetPointer());
        Assert(target != nullptr);

        // override instance class
        target->GetObjectHeader_Internal()->cls = this;

        ObjectInitializerContext* context = GetGlobalContext<ObjectInitializerContext>();

        if ((!context || !(context->flags & ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)))
        {
            scriptObjectResource = target->GetScriptObjectResource();

            if (!scriptObjectResource)
            {
                scriptObjectResource = ScriptObjectFunctions::CreateScriptObjectResource_DotNet(target, managedClass);
                AssertDebug(scriptObjectResource != nullptr);

                target->SetScriptObjectResource(scriptObjectResource);
            }
            else
            {
                scriptObjectResource->SetScriptObjectData_DotNet(ScriptObjectData_DotNet { .managedClass = managedClass });
            }

            scriptObjectResource->AddReader();

            int64 readers, writers;
            scriptObjectResource->GetNumUsers(readers, writers);
            Assert(readers == 1);
        }

        isCreated = true;
    }
#endif

#ifdef HYP_SCRIPT
    if (IsEnumType())
    {
        HYP_NOT_IMPLEMENTED(); // enum instance creation not yet implemented for scripts
    }

    // get or create new container for dynamic type
    ObjectContainer<ObjectBase>* container = static_cast<ObjectContainer<ObjectBase>*>(GetObjectContainer());
    Assert(container != nullptr);

    Array<const Class*> dynamicParents;
    const Class* topParent = m_parent;

    if (m_parent != nullptr)
    {
        while (topParent != nullptr && topParent != g_clsObjectBase)
        {
            if (!topParent->IsDynamic())
            {
                // stop after first non-dynamic parent class
                HYP_LOG(Core, Error, "Non-dynamic parent class construction not yet implemented in HypScript for dynamic class {}, Parent class: {}",
                        GetName(), topParent->GetName());

                return isCreated;
            }

            dynamicParents.PushBack(topParent);

            topParent = topParent->GetParent();
        }
    }

    PushGlobalContext(ObjectInitializerContext { .cls = this, .flags = ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

    ObjectHeader* header = reinterpret_cast<ObjectHeader*>(reinterpret_cast<UIntPtr>(container->Allocate(m_size)) - sizeof(ObjectHeader));
    header->cls = this;

    ObjectBase* target = ObjectHeader::GetObjectPointer(header);
    new (target) ObjectBase();

    // where to start writing fields
    size_t fieldOffset = (topParent != nullptr && !topParent->IsDynamic() && topParent != g_clsObjectBase ? topParent->GetSize() : 0)
        + sizeof(ObjectBase);

    // Add reference for this, in ReleaseObject() will decrement the ref count.
    const_cast<DynamicClassInstance*>(this)->AddRef();

    for (size_t i = dynamicParents.Size(); i > 0; i--)
    {
        const Class* dynamicParent = dynamicParents[i - 1];
        AssertDebug(dynamicParent->IsDynamic(), "Expected dynamic parent class");

        const DynamicClassInstance* dynamicParentInstance = static_cast<const DynamicClassInstance*>(dynamicParent);

        // Init all fields to BoxedValue()
        for (Field* field : dynamicParentInstance->GetFields())
        {
            // align field offset
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(BoxedValue));

            AssertDebug(fieldOffset + sizeof(BoxedValue) <= m_size,
                        "Field offset out of bounds: {} + {} > {}", fieldOffset, sizeof(BoxedValue), m_size);

            BoxedValue* fieldPtr = (BoxedValue*)(UIntPtr(target) + fieldOffset);
            new (fieldPtr) BoxedValue();

            fieldOffset += sizeof(BoxedValue);
        }
    }

    // our own class's fields lastly
    for (Field* field : GetFields())
    {
        // align field offset
        fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(BoxedValue));
        AssertDebug(fieldOffset + sizeof(BoxedValue) <= m_size,
                    "Field offset out of bounds: {} + {} > {}", fieldOffset, sizeof(BoxedValue), m_size);

        BoxedValue* fieldPtr = (BoxedValue*)(UIntPtr(target) + fieldOffset);
        new (fieldPtr) BoxedValue();

        fieldOffset += sizeof(BoxedValue);
    }

    Handle<ObjectBase> handle;
    handle.ptr = target;

    BoxedValue obj(std::move(handle));
    out = obj;

    PopGlobalContext<ObjectInitializerContext>();

    // ObjectInitializerContext* context = GetGlobalContext<ObjectInitializerContext>();

    // if ((!context || !(context->flags & ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)))
    // {
    scriptObjectResource = target->GetScriptObjectResource();

    if (!scriptObjectResource)
    {
        scriptObjectResource = ScriptObjectFunctions::CreateScriptObjectResource_Script((ScriptInstance*)nullptr, target);
        Assert(scriptObjectResource != nullptr);

        target->SetScriptObjectResource(scriptObjectResource);
    }
    else
    {
        scriptObjectResource->SetScriptObjectData_HypScript(ScriptObjectData_HypScript { nullptr, target });
    }
    // }

    isCreated = true;
#endif

    return isCreated;
}

bool DynamicClassInstance::CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const
{
    HYP_NOT_IMPLEMENTED();
}

void DynamicClassInstance::SetField(uint32 index, Field* field)
{
    AssertDebug(field != nullptr, "Cannot set null field to DynamicClass {}", GetName());
    AssertDebug(!m_fieldsByName.Contains(field->GetName()), "Field with name {} already exists in DynamicClass {}", field->GetName(), GetName());

    if (index >= m_fields.Size())
    {
        m_fields.Resize(index + 1);
    }

    m_fields[index] = field;
    m_fieldsByName[field->GetName()] = field;
}

void DynamicClassInstance::SetMethod(uint32 index, Method* method)
{
    AssertDebug(method != nullptr, "Cannot set null method to DynamicClass {}", GetName());
    AssertDebug(!m_methodsByName.Contains(method->GetName()), "Method with name {} already exists in DynamicClass {}", method->GetName(), GetName());

    if (index >= m_methods.Size())
    {
        m_methods.Resize(index + 1);
    }

    m_methods[index] = method;
    m_methodsByName[method->GetName()] = method;
}

void DynamicClassInstance::SetProperty(uint32 index, Property* property)
{
    AssertDebug(property != nullptr, "Cannot set null property to DynamicClass {}", GetName());
    AssertDebug(!m_propertiesByName.Contains(property->GetName()), "Property with name {} already exists in DynamicClass {}", property->GetName(), GetName());

    if (index >= m_properties.Size())
    {
        m_properties.Resize(index + 1);
    }

    m_properties[index] = property;
    m_propertiesByName[property->GetName()] = property;
}

void DynamicClassInstance::SetConstant(uint32 index, StaticField* staticField)
{
    AssertDebug(staticField != nullptr, "Cannot set null static field to DynamicClass {}", GetName());
    AssertDebug(!m_staticFieldsByName.Contains(staticField->GetName()), "Static field with name {} already exists in DynamicClass {}", staticField->GetName(), GetName());

    if (index >= m_staticFields.Size())
    {
        m_staticFields.Resize(index + 1);
    }

    m_staticFields[index] = staticField;
    m_staticFieldsByName[staticField->GetName()] = staticField;
}

void DynamicClassInstance::AddRef()
{
    AtomicIncrement(&m_refCount);
}

void DynamicClassInstance::Release()
{
    if (AtomicDecrement(&m_refCount) <= 0)
    {
        if (!(GetFlags() & ClassFlags::ANONYMOUS) && !ClassRegistry::GetInstance().Unregister(this))
        {
            HYP_LOG(Object, Warning, "Failed to unregister dynamic Class \"{}\"", GetName());
        }

        delete this;
    }
}

#pragma endregion DynamicClassInstance

#pragma region ClassRef

static inline bool IsDynamicClass(const Class& cls)
{
    return (cls.GetFlags() & (ClassFlags::DYNAMIC | ClassFlags::CLASS_TYPE)) == (ClassFlags::DYNAMIC | ClassFlags::CLASS_TYPE);
}

static inline bool IsDynamicStruct(const Class& cls)
{
    return (cls.GetFlags() & (ClassFlags::DYNAMIC | ClassFlags::STRUCT_TYPE)) == (ClassFlags::DYNAMIC | ClassFlags::STRUCT_TYPE);
}

static inline bool IsDynamicType(const Class& cls)
{
    return IsDynamicClass(cls) || IsDynamicStruct(cls);
}

static inline void DynamicType_AddRef(const Class* cls)
{
    if (IsDynamicClass(*cls))
    {
        const_cast<DynamicClassInstance*>(static_cast<const DynamicClassInstance*>(cls))->AddRef();
    }
    else if (IsDynamicStruct(*cls))
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(cls))->AddRef();
    }
}

static inline void DynamicType_Release(const Class* cls)
{
    if (IsDynamicClass(*cls))
    {
        const_cast<DynamicClassInstance*>(static_cast<const DynamicClassInstance*>(cls))->Release();
    }
    else if (IsDynamicStruct(*cls))
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(cls))->Release();
    }
}

ClassRef::ClassRef(const Class* cls, int initialRefCount)
    : cls(cls)
{
    if (cls && IsDynamicType(*cls) && initialRefCount > 0)
    {
        DynamicType_AddRef(cls);
    }
}

ClassRef::ClassRef(const ClassRef& other)
    : cls(other.cls)
{
    if (cls && IsDynamicType(*cls))
    {
        DynamicType_AddRef(cls);
    }
}

ClassRef& ClassRef::operator=(const ClassRef& other)
{
    if (this == &other || cls == other.cls)
    {
        return *this;
    }

    if (cls && IsDynamicType(*cls))
    {
        DynamicType_Release(cls);
    }

    cls = other.cls;

    if (cls && IsDynamicType(*cls))
    {
        DynamicType_AddRef(cls);
    }

    return *this;
}

ClassRef::ClassRef(ClassRef&& other) noexcept
    : cls(other.cls)
{
    other.cls = nullptr;
}

ClassRef& ClassRef::operator=(ClassRef&& other) noexcept
{
    if (this == &other || cls == other.cls)
    {
        return *this;
    }

    if (cls && IsDynamicType(*cls))
    {
        DynamicType_Release(cls);
    }

    cls = other.cls;
    other.cls = nullptr;

    return *this;
}

ClassRef::~ClassRef()
{
    if (cls && IsDynamicType(*cls))
    {
        DynamicType_Release(cls);
    }
}

#pragma endregion ClassRef

} // namespace Hyperion
