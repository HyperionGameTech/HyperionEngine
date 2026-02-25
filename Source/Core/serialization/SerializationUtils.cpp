/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/serialization/SerializationUtils.hpp>

#include <Core/json/JSON.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/Property.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/StaticField.hpp>
#include <Core/reflection/Method.hpp>
#include <Core/reflection/BoxedValue.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/LinkedList.hpp>

#include <Core/utilities/Format.hpp>
#include <Core/utilities/Uuid.hpp>
#include <Core/utilities/DeferredScope.hpp>
#include <Core/reflection/TypeInfo.hpp>
#include <Core/utilities/Float16.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>

#include <Core/utilities/GlobalContext.hpp>

// temp
#include <scene/Scene.hpp>
#endif

namespace Hyperion {

struct LoadAssetsFromReferencesContext
{
};

// used to prevent infinite recursion when serializing nested objects
static thread_local HashSet<Pair<TypeId, const void*>> s_serializedObjects;

// If true, class names will always be written when serializing objects IF the type != the declared type.
// For example, we're serializing an array of Animal and we encounter a Dog object, we need to write the class name
// otherwise we won't know to deserialize it as a Dog.
static constexpr bool ForceWriteClassNamesWhenTypesDiffer = true;

bool BoxedToJSON(
    const BoxedValue& value,
    JSON::Value& outJson,
    ToJSONOptions opts)
{
    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    if (value.IsNull())
    {
        outJson = JSON::JSNull();

        return true;
    }

    const TypeInfo& typeInfo = *value.GetTypeInfo();

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    AssetReference assetReference;
    bool isAssetObject = false;

    if (value.Is<AssetReference>())
    {
        assetReference = value.Get<AssetReference>();
    }
    else if (value.Is<AssetPath>() && opts.followAssetPaths >= ToJSONOptions::FollowAssetPathsMode::MatchingAttribute)
    {
        assetReference = AssetReference(value.Get<AssetPath>());
    }
    else if (value.Is<AssetObject>())
    {
        // Serialize AssetObject deriving classes by their asset reference if we're saving an Editor project.
        const AssetObject& assetObject = value.Get<AssetObject>();
        AssertDebug(assetObject.IsRegistered(), "Cannot serialize unregistered AssetObject {}", assetObject.GetName());

        if (!assetObject.IsRegistered())
        {
            HYP_LOG(Core, Warning, "Cannot serialize unregistered AssetObject!");
            return false;
        }

        assetReference = AssetReference(assetObject.HandleFromThis());
        isAssetObject = true;
    }

    if (isAssetObject) // && opts.saveAssetsAsReferences >= ToJSONOptions::SaveAssetsAsReferencesMode::UnlessOtherwiseSpecified)
    {
        AssertDebug(assetReference.IsValid(), "Serializing invalid asset reference");

        return BoxedToJSON(BoxedValue(assetReference), outJson, opts);
    }

    assetReference = AssetReference(); // reset
#endif

    if (value.Is<bool>(/* strict */ true))
    {
        outJson = value.Get<bool>();

        return true;
    }

#define DO_NUMERIC_TYPE(T)                        \
    if (value.Is<T>(/* strict */ true))           \
    {                                             \
        outJson = JSON::Number(value.Get<T>()); \
                                                  \
        return true;                              \
    }

    DO_NUMERIC_TYPE(int8);
    DO_NUMERIC_TYPE(int16);
    DO_NUMERIC_TYPE(int32);
    DO_NUMERIC_TYPE(int64);

    DO_NUMERIC_TYPE(uint8);
    DO_NUMERIC_TYPE(uint16);
    DO_NUMERIC_TYPE(uint32);
    DO_NUMERIC_TYPE(uint64);

    DO_NUMERIC_TYPE(Float16);
    DO_NUMERIC_TYPE(float);
    DO_NUMERIC_TYPE(double);

#ifdef HYP_WINDOWS
    if constexpr (!std::is_same_v<SizeType, uint64> && !std::is_same_v<SizeType, uint32>)
    {
        DO_NUMERIC_TYPE(SizeType);
    }
#endif

#undef DO_NUMERIC_TYPE

    if (value.Is<String>())
    {
        outJson = JSON::JString(value.Get<String>());

        return true;
    }

    if (typeInfo.IsVectorType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VECTOR);

        ITypeInfoVectorHandler* handler = static_cast<ITypeInfoVectorHandler*>(typeInfo.extendedInfo.handler);

        const int size = handler->GetNumComponents();

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        for (int i = 0; i < size; i++)
        {
            const AnyRef element = handler->GetComponent(value, i);

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of vector of type {}", i, typeInfo.name);

                continue;
            }

            JSON::Value jsonValue;
            if (!BoxedToJSON(BoxedValue(element), jsonValue, opts))
            {
                return false;
            }

            jsonArray.PushBack(std::move(jsonValue));
        }

        outJson = std::move(jsonArray);

        return true;
    }

    if (typeInfo.IsMatrixType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_MATRIX);

        ITypeInfoMatrixHandler* handler = static_cast<ITypeInfoMatrixHandler*>(typeInfo.extendedInfo.handler);

        const int rows = handler->GetNumRows();
        const int columns = handler->GetNumColumns();

        JSON::JArray jsonArray;
        jsonArray.Resize(rows * columns);

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < columns; c++)
            {
                const AnyRef element = handler->GetElement(value, r, c);

                if (!element.HasValue())
                {
                    HYP_LOG(Core, Warning, "Failed to get element at ({}, {}) of matrix of type {}", r, c, typeInfo.name);

                    return false;
                }

                JSON::Value jsonValue;
                if (!BoxedToJSON(BoxedValue(element), jsonValue, opts))
                {
                    return false;
                }

                jsonArray[r * columns + c] = std::move(jsonValue);
            }
        }

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<UUID>())
    {
        const UUID& uuid = value.Get<UUID>();

        outJson = JSON::JString(uuid.ToString());

        return true;
    }

    if (value.Is<Name>())
    {
        const Name name = value.Get<Name>();

        outJson = JSON::JString(name.LookupString());

        return true;
    }

    if (typeInfo.IsArrayType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);

        const SizeType size = handler->GetSize(value);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

            if (!handler->GetElementAt(value, i, element))
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of array of type {}", i, typeInfo.name);

                continue;
            }

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->GetClass() != element.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            JSON::Value jsonValue;
            if (!BoxedToJSON(element, jsonValue, newOpts))
            {
                return false;
            }

            jsonArray.PushBack(std::move(jsonValue));
        }

        outJson = std::move(jsonArray);

        return true;
    }

    if (typeInfo.IsSetType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* handler = static_cast<ITypeInfoSetHandler*>(typeInfo.extendedInfo.handler);

        const SizeType size = handler->GetSize(value);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        // Use iterator to traverse set elements
        ITypeInfoIterator* iterator = handler->CreateIterator(value);
        HYP_DEFER({ delete iterator; });

        if (!iterator)
        {
            HYP_LOG(Core, Warning, "Failed to create iterator for set of type {}", typeInfo.name);
            return false;
        }

        while (iterator->HasNext())
        {
            AnyRef element = iterator->GetCurrent();

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get current element from set iterator of type {}", typeInfo.name);
                iterator->Next();
                continue;
            }

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            BoxedValue elementData(element);

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->GetClass() != elementData.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            JSON::Value jsonValue;
            if (!BoxedToJSON(elementData, jsonValue, newOpts))
            {
                return false;
            }

            jsonArray.PushBack(std::move(jsonValue));

            iterator->Next();
        }

        outJson = std::move(jsonArray);

        return true;
    }

    if (typeInfo.IsEnum() || typeInfo.IsEnumFlags())
    {
        const TypeInfo* underlyingTypeInfo = typeInfo.GetUnderlyingType();
        AssertDebug(underlyingTypeInfo != nullptr, "Enum type must have an underlying type");

        constexpr int64 MaxDblValue = std::bit_cast<int64>(DBL_MAX);
        constexpr uint64 MaxDblValueUnsigned = std::bit_cast<uint64>(DBL_MAX);

        if (underlyingTypeInfo->id == TypeId::ForType<uint8>()
            || underlyingTypeInfo->id == TypeId::ForType<uint16>()
            || underlyingTypeInfo->id == TypeId::ForType<uint32>()
            || underlyingTypeInfo->id == TypeId::ForType<uint64>())
        {

            if (value.Get<uint64>() > MaxDblValueUnsigned)
            {
                HYP_LOG(Core, Warning, "Enum value {} of type {} is too large to be represented as a JSON number without loss of precision", value.Get<uint64>(), typeInfo.name);
            }

            outJson = JSON::Number(value.Get<uint64>());

            return true;
        }
        else if (underlyingTypeInfo->id == TypeId::ForType<int8>()
            || underlyingTypeInfo->id == TypeId::ForType<int16>()
            || underlyingTypeInfo->id == TypeId::ForType<int32>()
            || underlyingTypeInfo->id == TypeId::ForType<int64>())
        {
            if (value.Get<int64>() > MaxDblValue || value.Get<int64>() < -MaxDblValue)
            {
                HYP_LOG(Core, Warning, "Enum value {} of type {} is too large to be represented as a JSON number without loss of precision", value.Get<int64>(), typeInfo.name);
            }

            outJson = JSON::Number(value.Get<int64>());

            return true;
        }

        HYP_LOG(Core, Warning, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);

        return false;
    }

    if (typeInfo.IsVariantType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VARIANT);

        ITypeInfoVariantHandler* handler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);

        if (handler->GetCurrentTypeIndex(value) == Variant<std::nullptr_t>::invalidTypeIndex)
        {
            // no value set, fine
            outJson = JSON::JSNull();

            return true;
        }

        AnyRef activeValue = handler->GetValue(value);

        if (!activeValue.HasValue())
        {
            HYP_LOG(Core, Warning, "Failed to get active value of variant of type {}", typeInfo.name);

            return false;
        }

        ToJSONOptions newOpts = opts;
        newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

        // always (if ForceWriteClassNamesWhenTypesDiffer is true) write out class names for variants.
        if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer)
        {
            newOpts.writeClassNames = true;
        }

        if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
        {
            newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
        }

        /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
        {
            newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
        }*/

        return BoxedToJSON(BoxedValue(activeValue), outJson, newOpts);
    }

    const Class* cls = GetClass(value.GetTypeId());

    if (cls)
    {
        Pair<TypeId, const void*> pair = { value.GetTypeId(), value.ToRef().GetPointer() };

        if (!s_serializedObjects.Insert(pair).second)
        {
            HYP_LOG(Core, Warning, "Detected circular reference when serializing BoxedValue to JSON!");

            return false;
        }

        HYP_DEFER({
            s_serializedObjects.Erase(pair);
        });

        JSON::Object jsonObject;

        if (!ObjectToJSON(cls, value, jsonObject, opts))
        {
            return false;
        }

        outJson = std::move(jsonObject);

        return true;
    }

    HYP_LOG(Core, Warning, "Don't know how to serialize BoxedValue with type \"{}\" to JSON", typeInfo.name);

    return false;
}

bool ObjectToJSON(const Class* cls, const BoxedValue& target, JSON::Object& outJson, ToJSONOptions opts)
{
    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    HashSet<Name> usedMembers;

    const Class* originalClass = cls;

    while (cls != nullptr)
    {
        // look for signature void ToJSON(Value& out)
        if (const Method* toJsonMethod = cls->GetMethod("ToJSON"_sh, /* deep */ false))
        {
            if (toJsonMethod->GetParameters().Size() != 2
                || toJsonMethod->GetTypeId() != TypeId::Void()
                || toJsonMethod->GetParameters()[0].typeInfo->id != cls->GetTypeId()
                || toJsonMethod->GetParameters()[1].typeInfo->id != TypeId::ForType<JSON::Value>())
            {
                HYP_LOG(Core, Warning, "Class \"{}\" has a ToJSON method but it has an invalid signature", cls->GetName());
            }
            else
            {
                BoxedValue returnValue;
                toJsonMethod->Invoke(Array<BoxedValue*> { const_cast<BoxedValue*>(&target), &returnValue });

                if (returnValue.Is<JSON::Value>())
                {
                    JSON::Value& jsonValue = returnValue.Get<JSON::Value>();

                    if (jsonValue.IsObject())
                    {
                        JSON::Object& jsonObject = jsonValue.AsObject();
                        std::swap(outJson, jsonObject);
                        outJson.MergeDeep(jsonObject);

                        // skip normal serialization since we got the full object from ToJSON
                        continue;
                    }
                    else
                    {
                        HYP_LOG(Core, Warning, "ToJSON method of Class \"{}\" did not return a valid JSON object, got JSON value: {}", cls->GetName(), jsonValue.ToString());
                    }
                }
                else
                {
                    HYP_LOG(Core, Warning, "Failed to invoke ToJSON method of Class \"{}\", got type {} but expected Value!",
                        cls->GetName(), returnValue.GetTypeInfo()->name);
                }
            }
        }

        for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
        {
            if (opts.skipTransientProperties)
            {
                if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrTransient); attribute.IsValid() && attribute.GetBool())
                {
                    continue;
                }
            }

            if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                //  skip fields that are marked as properties otherwise they will be serialized twice
                continue;
            }

            if (member.IsDelegate())
            {
                // skip delegates
                continue;
            }

            if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            if (usedMembers.Contains(member.GetName()))
            {
                continue;
            }

            usedMembers.Insert(member.GetName());

            switch (member.GetMemberType())
            {
            case MemberType::Property:
            {
                const Property* property = static_cast<const Property*>(&member);

                BoxedValue value = property->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && property->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !property->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !property->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (!BoxedToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize property \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *property->GetName();

                if (const ClassAttributeValue& pathAttribute = property->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    outJson = std::move(temp.AsObject());
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case MemberType::Field:
            {
                const Field* field = static_cast<const Field*>(&member);

                BoxedValue value = field->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && field->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !field->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !field->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (!BoxedToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize field \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *field->GetName();

                if (const ClassAttributeValue& pathAttribute = field->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    JSON::Object& obj = temp.AsObject();

                    outJson = std::move(obj);
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case MemberType::StaticField:
            {
                const StaticField* staticField = static_cast<const StaticField*>(&member);

                BoxedValue value = staticField->Get();

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && staticField->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !staticField->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !staticField->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (!BoxedToJSON(staticField->Get(), jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize static field \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *staticField->GetName();

                if (const ClassAttributeValue& pathAttribute = staticField->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    outJson = std::move(temp.AsObject());
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            default:
                break;
            }
        }

        cls = cls->GetParent();
    }

    if (originalClass && opts.writeClassNames)
    {
        auto classIt = outJson.Find("$Class");
        if (classIt != outJson.End())
        {
            outJson.Erase(classIt);
        }

        // write class at the beginning of the object
        outJson.PushFront({ "$Class", *originalClass->GetName() });
    }

    return true;
}

bool ObjectFromJSON(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target)
{
    auto ResolveMember = [&target](const IMember& member, const JSON::Value& value) -> bool
    {
        switch (member.GetMemberType())
        {
        case MemberType::Property:
        {
            const Property& property = static_cast<const Property&>(member);

            if (!property.CanSet())
                return false;

            const TypeInfo& typeInfo = property.GetTypeInfo();

            BoxedValue boxed;

            if (!BoxedFromJSON(value, typeInfo, boxed))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize property \"{}\" from json",
                    member.GetName());

                return false;
            }

            if (boxed.IsNull())
            {
                // dont set null values
                return true;
            }

            property.Set(target, boxed);

            return true;
        }
        case MemberType::Field:
        {
            const Field& field = static_cast<const Field&>(member);
            const TypeInfo& typeInfo = field.GetTypeInfo();

            BoxedValue boxed;

            if (!BoxedFromJSON(value, typeInfo, boxed))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize field \"{}\" from json",
                    member.GetName());

                return false;
            }

            if (boxed.IsNull())
            {
                // dont set null values
                return true;
            }

            field.Set(target, boxed);

            return true;
        }
        default:
            break;
        }

        return false;
    };

    JSON::Value jsonObjectValue { jsonObject };

    const Class* instanceClass = target.GetTypeInfo()->GetClass();

    if (!instanceClass)
    {
        instanceClass = targetClass;
    }

    if (instanceClass != nullptr)
    {
        if (target.IsNull())
        {
            if (!instanceClass->CreateInstance(target, /* allowAbstract */ false))
            {
                return false;
            }
        }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        GlobalContextScope contextScope { LoadAssetsFromReferencesContext() };
#endif

        // members with LoadOrder attribute get binned and put here first
        SortedArray<KeyValuePair<int, const IMember*>> sortedMembers;

        // reoslve jsonpath members first
        for (const IMember& member : instanceClass->GetMembers(MemberType::Field | MemberType::Property))
        {
            if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            const ClassAttributeValue& pathAttribute = member.GetAttribute(Attributes::g_attrJsonPath);

            if (!pathAttribute.IsValid())
            {
                continue;
            }

            UTF8StringView path = pathAttribute.GetString();

            auto value = jsonObjectValue.Get(path);

            if (!value.value)
            {
                HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for Class \"{}\"", path, instanceClass->GetName());

                continue;
            }

            if (const ClassAttributeValue& sortOrderAttribute = member.GetAttribute(Attributes::g_attrLoadOrder); sortOrderAttribute.IsValid())
            {
                sortedMembers.Insert({ sortOrderAttribute.GetInt(), &member });

                continue;
            }

            // not sorted
            sortedMembers.Insert({ 0, &member });
        }

        for (const KeyValuePair<int, const IMember*>& pair : sortedMembers)
        {
            if (!ResolveMember(*pair.second, *jsonObjectValue.Get(pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString()).value))
            {
                HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for Class \"{}\"", pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString(), instanceClass->GetName());
                continue;
            }
        }

        sortedMembers.Clear();

        // resolve data from json object to members by name
        for (const auto& [key, value] : jsonObject)
        {
            const IMember* member = instanceClass->GetMember(key.ToUtf8(), MemberType::Property | MemberType::Field);

            if (!member)
            {
                // member not found, skip
                continue;
            }

            if (member->GetMemberType() != MemberType::Property && member->GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                //  skip fields that are marked as properties
                continue;
            }

            // skip members with jsonpath attribute - they were already resolved
            if (member->GetAttribute(Attributes::g_attrJsonPath).IsValid())
            {
                continue;
            }

            // skip if jsonignore is set
            if (const ClassAttributeValue& attribute = member->GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            if (const ClassAttributeValue& sortOrderAttribute = member->GetAttribute(Attributes::g_attrLoadOrder); sortOrderAttribute.IsValid())
            {
                sortedMembers.Insert({ sortOrderAttribute.GetInt(), member });

                continue;
            }

            // not sorted
            sortedMembers.Insert({ 0, member });
        }

        for (const KeyValuePair<int, const IMember*>& pair : sortedMembers)
        {
            if (!ResolveMember(*pair.second, *jsonObjectValue.Get(*pair.second->GetName()).value))
            {
                HYP_LOG(Core, Warning, "Failed to resolve member \"{}\" for Class \"{}\"", pair.second->GetName(), instanceClass->GetName());
                continue;
            }
        }
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (IsGlobalContextActive<LoadAssetsFromReferencesContext>() && target.Is<AssetReference>() && targetClass != AssetReference::StaticClass())
    {
        AssetReference& assetReference = target.Get<AssetReference>();

        // target wants an AssetPath, we can give it the AssetPath
        if (targetClass == AssetPath::StaticClass())
        {
            target = BoxedValue(assetReference.GetAssetPath());

            return true;
        }

        if (assetReference.IsValid())
        {
            if (Handle<AssetObject> assetObject = assetReference.Resolve(); assetObject.IsValid())
            {
                target = BoxedValue(std::move(assetObject));

                return true;
            }
            else
            {
                HYP_LOG(Core, Warning, "Failed to load AssetObject from AssetReference: {}", assetReference.GetAssetPath());

                target = BoxedValue(Handle<ObjectBase>::Null());

                return false;
            }
        }

        HYP_LOG(Core, Warning, "Failed to resolve AssetReference when deserializing to AssetObject: invalid reference at \"{}\"", assetReference.GetAssetPath().ToString());

        return false;
    }
#endif

    return true;
}

bool BoxedFromJSON(const JSON::Value& jsonValue, const TypeInfo& typeInfo, BoxedValue& outBoxed)
{
    if (typeInfo.IsBoolType())
    {
        if (!jsonValue.IsBool())
        {
            HYP_LOG(Core, Warning, "Expected JSON bool for bool but got: {}", jsonValue.ToString());
            return false;
        }

        outBoxed = BoxedValue(jsonValue.AsBool());
        return true;
    }

    if (typeInfo.IsIntegralType())
    {
        if (!jsonValue.IsNumber())
        {
            HYP_LOG(Core, Warning, "Expected JSON number for integral type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        const JSON::Number number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<int8>())
        {
            if (number < INT8_MIN || number > INT8_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int8", number);
                return false;
            }

            outBoxed = BoxedValue(int8(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int16>())
        {
            if (number < INT16_MIN || number > INT16_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int16", number);
                return false;
            }

            outBoxed = BoxedValue(int16(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int32>())
        {
            if (number < INT32_MIN || number > INT32_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int32", number);
                return false;
            }

            outBoxed = BoxedValue(int32(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int64>())
        {
            if (number < INT64_MIN || number > INT64_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int64", number);
                return false;
            }

            outBoxed = BoxedValue(int64(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint8>())
        {
            if (number < 0 || number > UINT8_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint8", number);
                return false;
            }

            outBoxed = BoxedValue(uint8(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint16>())
        {
            if (number < 0 || number > UINT16_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint16", number);
                return false;
            }

            outBoxed = BoxedValue(uint16(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint32>())
        {
            if (number < 0 || number > UINT32_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint32", number);
                return false;
            }

            outBoxed = BoxedValue(uint32(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint64>())
        {
            if (number < 0 || number > UINT64_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint64", number);
                return false;
            }

            outBoxed = BoxedValue(uint64(number));
            return true;
        }

#ifdef HYP_WINDOWS
        if constexpr (!std::is_same_v<SizeType, uint64> && !std::is_same_v<SizeType, uint32>)
        {
            if (typeInfo.id == TypeId::ForType<SizeType>())
            {
                if (number < 0 || number > SIZE_MAX)
                {
                    return false;
                }

                outBoxed = BoxedValue(SizeType(number));
                return true;
            }
        }
#endif

        return false;
    }

    if (typeInfo.IsFloatType())
    {
        if (!jsonValue.IsNumber())
        {
            HYP_LOG(Core, Warning, "Expected JSON number for float type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        const JSON::Number number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<Float16>())
        {
            // Clamp to representable range of Float16
            const double clamped = MathUtil::Clamp(double(number), double(-FLT16_MAX), double(FLT16_MAX));
            outBoxed = BoxedValue(Float16(float(MathUtil::Clamp(double(clamped), double(-FLT16_MAX), double(FLT16_MAX)))));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<float>())
        {
            outBoxed = BoxedValue(float(MathUtil::Clamp(double(number), double(-FLT_MAX), double(FLT_MAX))));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<double>())
        {
            outBoxed = BoxedValue(double(number));
            return true;
        }

        return false;
    }

    if (typeInfo.IsStringType())
    {
        if (!jsonValue.IsString())
        {
            HYP_LOG(Core, Warning, "Expected JSON string for string type {}, but got: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_STRING)
        {
            HYP_LOG(Core, Warning, "String type {} does not have a valid string handler", typeInfo.name);
            return false;
        }

        ITypeInfoStringHandler* stringHandler = static_cast<ITypeInfoStringHandler*>(handler);

        BoxedValue stringInstance;
        if (!stringHandler->CreateInstance(stringInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of string type");
            return false;
        }

        stringHandler->SetValue(stringInstance, jsonValue.AsString().ToUtf8());

        outBoxed = std::move(stringInstance);

        return true;
    }

    if (typeInfo.IsEnum() || typeInfo.IsEnumFlags())
    {
        const TypeInfo* underlyingTypeInfo = typeInfo.GetUnderlyingType();
        AssertDebug(underlyingTypeInfo != nullptr, "Enum type must have an underlying type");

        if (!underlyingTypeInfo)
        {
            HYP_LOG(Core, Warning, "Enum type {} does not have a valid underlying type", typeInfo.name);
            return false;
        }

        if (jsonValue.IsNumber())
        {
            if (underlyingTypeInfo->id == TypeId::ForType<int8>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt8());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int16>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt16());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int32>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt32());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int64>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt64());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint8>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt8());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint16>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt16());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint32>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt32());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint64>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt64());
                return true;
            }
            else
            {
                HYP_LOG(Core, Warning, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);
                return false;
            }
        }

        /// \todo String representation of enum values

        return false;
    }

    if (typeInfo.id == TypeId::ForType<UUID>())
    {
        if (!jsonValue.IsString())
        {
            HYP_LOG(Core, Warning, "Expected JSON string for UUID, but got value: {}", jsonValue.ToString());
            return false;
        }

        const JSON::JString& jsonString = jsonValue.AsString();

        if (jsonString.Size() != 36)
        {
            HYP_LOG(Core, Warning, "Invalid UUID string length: {}", jsonString.Size());
            return false;
        }

        outBoxed = BoxedValue(UUID(*jsonString.ToAnsi()));
        return true;
    }

    if (typeInfo.id == TypeId::ForType<Name>())
    {
        outBoxed = BoxedValue(Name(CreateNameFromDynamicString(*jsonValue.ToString().ToAnsi())));
        return true;
    }

    if (typeInfo.IsVectorType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for vector type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_VECTOR)
        {
            HYP_LOG(Core, Warning, "Vector type {} does not have a valid vector handler", typeInfo.name);
            return false;
        }

        ITypeInfoVectorHandler* vectorHandler = static_cast<ITypeInfoVectorHandler*>(handler);

        BoxedValue vectorInstance;
        if (!vectorHandler->CreateInstance(vectorInstance))
        {
            return false;
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != vectorHandler->GetNumComponents())
        {
            HYP_LOG(Core, Warning, "Expected JSON array of size {} for vector type {}, but got size {}",
                vectorHandler->GetNumComponents(), typeInfo.name, jsonArray.Size());
            return false;
        }

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Vector type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        for (int i = 0; i < int(jsonArray.Size()); i++)
        {
            BoxedValue elementData;

            if (!BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize vector element at index {} for type {}", i, typeInfo.name);

                return false;
            }

            vectorHandler->SetComponent(vectorInstance, i, elementData);
        }

        outBoxed = std::move(vectorInstance);

        return true;
    }

    if (typeInfo.IsMatrixType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for matrix type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_MATRIX)
        {
            HYP_LOG(Core, Warning, "Matrix type {} does not have a valid matrix handler", typeInfo.name);
            return false;
        }

        ITypeInfoMatrixHandler* matrixHandler = static_cast<ITypeInfoMatrixHandler*>(handler);

        BoxedValue matrixInstance;
        if (!matrixHandler->CreateInstance(matrixInstance))
        {
            return false;
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != matrixHandler->GetNumRows() * matrixHandler->GetNumColumns())
        {
            HYP_LOG(Core, Warning, "Expected JSON array of size {} for matrix type {}, but got size {}",
                matrixHandler->GetNumRows() * matrixHandler->GetNumColumns(), typeInfo.name, jsonArray.Size());
            return false;
        }

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Matrix type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            BoxedValue elementData;

            if (!BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize matrix element at index {} for type {}", i, typeInfo.name);

                return false;
            }

            matrixHandler->SetElement(matrixInstance, i / matrixHandler->GetNumColumns(), i % matrixHandler->GetNumColumns(), elementData);
        }

        outBoxed = std::move(matrixInstance);

        return true;
    }

    if (typeInfo.IsArrayType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for array type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());

            return false;
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Array type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* arrayHandler = static_cast<ITypeInfoArrayHandler*>(handler);

        BoxedValue arrayInstance;
        if (!arrayHandler->CreateInstance(arrayInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of array type {}", typeInfo.name);

            return false;
        }

        arrayHandler->Resize(arrayInstance, jsonArray.Size());

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            if (jsonArray[i].IsNull())
            {
                // skip null/undefined elements
                continue;
            }

            BoxedValue elementData;

            if (!BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize array element at index {} for type: {}", i, typeInfo.name);

                return false;
            }

            if (!arrayHandler->SetElementAt(arrayInstance, i, std::move(elementData)))
            {
                HYP_LOG(Core, Warning, "Failed to set array element at index {} for type: {}", i, typeInfo.name);

                return false;
            }
        }

        outBoxed = std::move(arrayInstance);

        return true;
    }

    if (typeInfo.IsSetType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for set type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());

            return false;
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Set type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* setHandler = static_cast<ITypeInfoSetHandler*>(handler);

        BoxedValue setInstance;
        if (!setHandler->CreateInstance(setInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of set type {}", typeInfo.name);

            return false;
        }

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            if (jsonArray[i].IsNull())
            {
                // skip null/undefined elements
                continue;
            }

            BoxedValue elementData;

            if (!BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize set element at index {} for type: {}", i, typeInfo.name);

                return false;
            }

            if (!setHandler->Insert(setInstance, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to insert set element at index {} for type: {} (may be duplicate)", i, typeInfo.name);

                // Continue anyway - duplicates are expected behavior for sets
                continue;
            }
        }

        outBoxed = std::move(setInstance);

        return true;
    }

    if (typeInfo.IsVariantType())
    {
        // variant type is stored directly as whatever inner type it was serialized as.
        // so, we need to iterate over the possible types and stop at the first one that works.

        ITypeInfoVariantHandler* variantHandler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);
        Assert(variantHandler && variantHandler->GetHandlerType() == ITypeInfoHandler::TYPE_VARIANT);

        if (jsonValue.IsNull())
        {
            // create empty variant on null/undefined

            BoxedValue variantInstance;
            if (!variantHandler->CreateInstance(variantInstance))
            {
                HYP_LOG(Core, Warning, "Failed to create instance of variant type {}", typeInfo.name);
                return false;
            }

            outBoxed = std::move(variantInstance);

            return true;
        }

        const int numTypes = variantHandler->GetNumTypes();

        for (int typeIndex = 0; typeIndex < numTypes; typeIndex++)
        {
            const TypeInfo* variantTypeInfo = variantHandler->GetTypeInfoAtIndex(typeIndex);
            AssertDebug(variantTypeInfo != nullptr, "Variant type info at index {} is null", typeIndex);

            if (!variantTypeInfo)
            {
                continue;
            }

            BoxedValue variantData;

            if (BoxedFromJSON(jsonValue, *variantTypeInfo, variantData))
            {
                BoxedValue variantInstance;

                if (!variantHandler->CreateInstance(variantInstance))
                {
                    HYP_LOG(Core, Warning, "Failed to create instance of variant type {}", typeInfo.name);
                    return false;
                }

                if (!variantHandler->SetValue(variantInstance, variantData))
                {
                    HYP_LOG(Core, Warning, "Failed to set value of variant type {}", typeInfo.name);
                    return false;
                }

                outBoxed = std::move(variantInstance);
                return true;
            }
        }

        HYP_LOG(Core, Warning, "Failed to deserialize JSON to any of the possible types for variant: {}", typeInfo.name);

        return false;
    }

    if (jsonValue.IsNull() && typeInfo.IsClass())
    {
        // null object
        outBoxed = BoxedValue();

        return true;
    }

    // Object types
    if (jsonValue.IsObject())
    {
        const Class* typeInfoClass = typeInfo.GetClass();
        const Class* instanceClass = typeInfoClass;

        // first check for $Class property to override type
        if (auto classValue = jsonValue.Get("$Class"); classValue && classValue.IsString())
        {
            const Class* derivedClass = GetClass(StringHash(classValue.AsString()));

            if (derivedClass)
            {
                if (instanceClass && !derivedClass->IsDerivedFrom(instanceClass))
                {
                    HYP_LOG(Core, Warning, "Class '{}' is not derived from expected class '{}'", derivedClass->GetName(), instanceClass->GetName());
                }
            }
            else
            {
                HYP_LOG(Core, Warning, "Class '{}' not found!", classValue.AsString());
            }

            instanceClass = derivedClass;
        }

        if (instanceClass)
        {
            BoxedValue instance;

            if (!instanceClass->CreateInstance(instance))
            {
                HYP_LOG(Core, Warning, "Failed to create instance of Class \"{}\"", instanceClass->GetName());
                return false;
            }

            if (!ObjectFromJSON(jsonValue.AsObject(), typeInfoClass, instance))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize instance of \"{}\" from JSON", instanceClass->GetName());
                return false;
            }

            outBoxed = std::move(instance);

            return true;
        }

        HYP_LOG(Core, Warning, "Could not find Class for type: {}", typeInfo.name);

        return false;
    }

    HYP_LOG(Core, Warning, "Failed to deserialize JSON to BoxedValue of type: {}, no handle logic for JSON value: {}",
        typeInfo.name, jsonValue.ToString(true));

    return false;
}

} // namespace Hyperion
