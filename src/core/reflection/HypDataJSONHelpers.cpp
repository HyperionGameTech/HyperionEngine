/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypDataJSONHelpers.hpp>

#include <core/json/JSON.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/HypProperty.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/HypConstant.hpp>
#include <core/reflection/HypMethod.hpp>
#include <core/reflection/HypData.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/reflection/TypeInfo.hpp>
#include <core/utilities/Float16.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>

#include <core/utilities/GlobalContext.hpp>
#endif

namespace hyperion {

struct SaveAssetsAsReferencesContext
{
};

struct LoadAssetsFromReferencesContext
{
};

// used to prevent infinite recursion when serializing nested objects
static thread_local HashSet<Pair<TypeId, const void*>> s_serializedObjects;

// If true, class names will always be written when serializing objects IF the type != the declared type.
// For example, we're serializing an array of Animal and we encounter a Dog object, we need to write the class name
// otherwise we won't know to deserialize it as a Dog.
static constexpr bool ForceWriteClassNamesWhenTypesDiffer = true;

bool HypDataToJSON(
    const HypData& value,
    json::JSONValue& outJson,
    ToJSONOptions opts)
{
    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    if (value.IsNull())
    {
        outJson = json::JSONNull();

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
    else if (value.Is<AssetObject>())
    {
        // Serialize AssetObject deriving classes by their asset reference if we're saving an Editor project.
        const AssetObject& assetObject = value.Get<AssetObject>();
        AssertDebug(assetObject.IsRegistered(), "Cannot serialize unregistered AssetObject");

        if (!assetObject.IsRegistered())
        {
            HYP_LOG(Core, Warning, "Cannot serialize unregistered AssetObject!");
            return false;
        }

        assetReference = AssetReference(assetObject.HandleFromThis());
        isAssetObject = true;
    }

    if (isAssetObject && assetReference.IsValid() && opts.saveAssetObjectsAsReferences && IsGlobalContextActive<SaveAssetsAsReferencesContext>())
    {
        return HypDataToJSON(HypData(assetReference), outJson, opts);
    }

    assetReference = AssetReference(); // reset
#endif

    if (value.Is<bool>(/* strict */ true))
    {
        outJson = json::JSONBool(value.Get<bool>());

        return true;
    }

#define DO_NUMERIC_TYPE(T)                          \
    if (value.Is<T>(/* strict */ true))             \
    {                                               \
        outJson = json::JSONNumber(value.Get<T>()); \
                                                    \
        return true;                                \
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
        outJson = json::JSONString(value.Get<String>());

        return true;
    }

    if (typeInfo.IsVectorType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VECTOR);

        ITypeInfoVectorHandler* handler = static_cast<ITypeInfoVectorHandler*>(typeInfo.extendedInfo.handler);

        const int size = handler->GetNumComponents();

        json::JSONArray jsonArray;
        jsonArray.Reserve(size);

        for (int i = 0; i < size; i++)
        {
            const AnyRef element = handler->GetComponent(value, i);

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of vector of type {}", i, typeInfo.name);

                continue;
            }

            json::JSONValue jsonValue;
            if (!HypDataToJSON(HypData(element), jsonValue, opts))
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

        json::JSONArray jsonArray;
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

                json::JSONValue jsonValue;
                if (!HypDataToJSON(HypData(element), jsonValue, opts))
                {
                    return false;
                }

                jsonArray[r * columns + c] = std::move(jsonValue);
            }
        }

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Uuid>())
    {
        const Uuid& uuid = value.Get<Uuid>();

        outJson = json::JSONString(uuid.ToString());

        return true;
    }

    if (value.Is<Name>())
    {
        const Name name = value.Get<Name>();

        outJson = json::JSONString(name.LookupString());

        return true;
    }

    if (typeInfo.IsArrayType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);

        const SizeType size = handler->GetSize(value);

        json::JSONArray jsonArray;
        jsonArray.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

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

            json::JSONValue jsonValue;
            if (!HypDataToJSON(element, jsonValue, newOpts))
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

        json::JSONArray jsonArray;
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

            HypData elementData(element);

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->GetClass() != elementData.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            json::JSONValue jsonValue;
            if (!HypDataToJSON(elementData, jsonValue, newOpts))
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

            outJson = json::JSONNumber(value.Get<uint64>());

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

            outJson = json::JSONNumber(value.Get<int64>());

            return true;
        }

        HYP_LOG(Core, Warning, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);

        return false;
    }

    const Class* cls = GetClass(value.GetTypeId());

    if (cls)
    {
        Pair<TypeId, const void*> pair = { value.GetTypeId(), value.ToRef().GetPointer() };

        if (!s_serializedObjects.Insert(pair).second)
        {
            HYP_LOG(Core, Warning, "Detected circular reference when serializing HypData to JSON!");

            HYP_BREAKPOINT_DEBUG_MODE;

            return false;
        }

        HYP_DEFER({
            s_serializedObjects.Erase(pair);
        });

        json::JSONObject jsonObject;

        GlobalContextScope contextScope { SaveAssetsAsReferencesContext() };

        if (!ObjectToJSON(cls, value, jsonObject, opts))
        {
            return false;
        }

        outJson = std::move(jsonObject);

        return true;
    }

    HYP_LOG(Core, Warning, "Don't know how to serialize HypData with type \"{}\" to JSON", typeInfo.name);

    return false;
}

bool ObjectToJSON(
    const Class* cls,
    const HypData& target,
    json::JSONObject& outJson,
    ToJSONOptions opts)
{
    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    HashSet<Name> usedMembers;

    const Class* originalClass = cls;

    while (cls != nullptr)
    {
        // look for signature void ToJSON(JSONValue& out)
        if (const HypMethod* toJsonMethod = cls->GetMethod("ToJSON", /* deep */ false))
        {
            if (toJsonMethod->GetParameters().Size() != 2
                || toJsonMethod->GetTypeId() != TypeId::Void()
                || toJsonMethod->GetParameters()[0].typeInfo->id != cls->GetTypeId()
                || toJsonMethod->GetParameters()[1].typeInfo->id != TypeId::ForType<json::JSONValue>())
            {
                HYP_LOG(Core, Warning, "Class \"{}\" has a ToJSON method but it has an invalid signature", cls->GetName());
            }
            else
            {
                HypData returnValue;
                toJsonMethod->Invoke(Array<HypData*> { const_cast<HypData*>(&target), &returnValue });

                if (returnValue.Is<json::JSONValue>())
                {
                    json::JSONValue& jsonValue = returnValue.Get<json::JSONValue>();

                    if (jsonValue.IsObject())
                    {
                        json::JSONObject& jsonObject = jsonValue.AsObject();
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
                    HYP_LOG(Core, Warning, "Failed to invoke ToJSON method of Class \"{}\", got type {} but expected JSONValue!",
                        cls->GetName(), returnValue.GetTypeInfo()->name);
                }
            }
        }

        for (const IHypMember& member : cls->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY, /* deep */ false))
        {
            if (opts.skipTransientProperties)
            {
                if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrTransient); attribute.IsValid() && attribute.GetBool())
                {
                    continue;
                }
            }

            if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                //  skip fields that are marked as properties otherwise they will be serialized twice
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
            case HypMemberType::TYPE_PROPERTY:
            {
                const HypProperty* property = static_cast<const HypProperty*>(&member);

                HypData value = property->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && property->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize property \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *property->GetName();

                if (const ClassAttributeValue& pathAttribute = property->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    json::JSONValue temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    outJson = std::move(temp.AsObject());
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case HypMemberType::TYPE_FIELD:
            {
                const Field* field = static_cast<const Field*>(&member);

                HypData value = field->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && field->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize field \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *field->GetName();

                if (const ClassAttributeValue& pathAttribute = field->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    json::JSONValue temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    json::JSONObject& obj = temp.AsObject();

                    outJson = std::move(obj);
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case HypMemberType::TYPE_CONSTANT:
            {
                const HypConstant* constant = static_cast<const HypConstant*>(&member);

                HypData value = constant->Get();

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && constant->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(constant->Get(), jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize constant \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *constant->GetName();

                if (const ClassAttributeValue& pathAttribute = constant->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    json::JSONValue temp(std::move(outJson));
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

bool JSONToObject(const json::JSONObject& jsonObject, const Class* targetClass, HypData& target)
{
    auto resolveMember = [&target](const IHypMember& member, const json::JSONValue& value) -> bool
    {
        switch (member.GetMemberType())
        {
        case HypMemberType::TYPE_PROPERTY:
        {
            const HypProperty& property = static_cast<const HypProperty&>(member);
            const TypeInfo& typeInfo = property.GetTypeInfo();

            HypData hypData;

            if (!JSONToHypData(value, typeInfo, hypData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize property \"{}\" from json",
                    member.GetName());

                return false;
            }

            property.Set(target, hypData);

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const Field& field = static_cast<const Field&>(member);
            const TypeInfo& typeInfo = field.GetTypeInfo();

            HypData hypData;

            if (!JSONToHypData(value, typeInfo, hypData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize field \"{}\" from json",
                    member.GetName());

                return false;
            }

            field.Set(target, hypData);

            break;
        }
        default:
            break;
        }

        return true;
    };

    json::JSONValue jsonObjectValue { jsonObject };

    const Class* instanceClass = target.GetTypeInfo()->GetClass();

    if (instanceClass != nullptr)
    {
#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        GlobalContextScope contextScope { LoadAssetsFromReferencesContext() };
#endif

        // members with LoadOrder attribute get binned and put here first
        SortedArray<KeyValuePair<int, const IHypMember*>> sortedMembers;

        // reoslve jsonpath members first
        for (const IHypMember& member : instanceClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY))
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

            const String& path = pathAttribute.GetString();

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

        for (const KeyValuePair<int, const IHypMember*>& pair : sortedMembers)
        {
            if (!resolveMember(*pair.second, *jsonObjectValue.Get(pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString()).value))
            {
                HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for Class \"{}\"", pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString(), instanceClass->GetName());
                continue;
            }
        }

        sortedMembers.Clear();

        // resolve data from json object to members by name
        for (const auto& [key, value] : jsonObject)
        {
            const IHypMember* member = instanceClass->GetMember(key, HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD);

            if (!member)
            {
                // member not found, skip
                continue;
            }

            if (member->GetMemberType() != HypMemberType::TYPE_PROPERTY && member->GetAttribute(Attributes::g_attrProperty).IsValid())
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

        for (const KeyValuePair<int, const IHypMember*>& pair : sortedMembers)
        {
            if (!resolveMember(*pair.second, *jsonObjectValue.Get(*pair.second->GetName()).value))
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

        if (assetReference.IsValid())
        {
            if (Handle<AssetObject> assetObject = assetReference.Resolve(); assetObject.IsValid())
            {
                target = HypData(std::move(assetObject));

                return true;
            }
            else
            {
                HYP_LOG(Core, Warning, "Failed to load AssetObject from AssetReference: {}", assetReference.GetAssetPath());

                target = HypData(AnyHandle(targetClass, nullptr));

                return false;
            }
        }

        HYP_LOG(Core, Warning, "Failed to resolve AssetReference when deserializing to AssetObject: invalid reference");

        return false;
    }
#endif

    return true;
}

bool JSONToHypData(const json::JSONValue& jsonValue, const TypeInfo& typeInfo, HypData& outHypData)
{
    if (typeInfo.IsBoolType())
    {
        if (!jsonValue.IsBool())
        {
            HYP_LOG(Core, Warning, "Expected JSON bool for bool but got: {}", jsonValue.ToString());
            return false;
        }

        outHypData = HypData(jsonValue.AsBool());
        return true;
    }

    if (typeInfo.IsIntegralType())
    {
        if (!jsonValue.IsNumber())
        {
            HYP_LOG(Core, Warning, "Expected JSON number for integral type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
            return false;
        }

        const json::JSONNumber number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<int8>())
        {
            if (number < INT8_MIN || number > INT8_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int8", number);
                return false;
            }

            outHypData = HypData(int8(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int16>())
        {
            if (number < INT16_MIN || number > INT16_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int16", number);
                return false;
            }

            outHypData = HypData(int16(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int32>())
        {
            if (number < INT32_MIN || number > INT32_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int32", number);
                return false;
            }

            outHypData = HypData(int32(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<int64>())
        {
            if (number < INT64_MIN || number > INT64_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for int64", number);
                return false;
            }

            outHypData = HypData(int64(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint8>())
        {
            if (number < 0 || number > UINT8_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint8", number);
                return false;
            }

            outHypData = HypData(uint8(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint16>())
        {
            if (number < 0 || number > UINT16_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint16", number);
                return false;
            }

            outHypData = HypData(uint16(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint32>())
        {
            if (number < 0 || number > UINT32_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint32", number);
                return false;
            }

            outHypData = HypData(uint32(number));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<uint64>())
        {
            if (number < 0 || number > UINT64_MAX)
            {
                HYP_LOG(Core, Warning, "Number {} out of range for uint64", number);
                return false;
            }

            outHypData = HypData(uint64(number));
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

                outHypData = HypData(SizeType(number));
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

        const json::JSONNumber number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<Float16>())
        {
            // Clamp to representable range of Float16
            const double clamped = MathUtil::Clamp(double(number), double(-FLT16_MAX), double(FLT16_MAX));
            outHypData = HypData(Float16(float(MathUtil::Clamp(double(clamped), double(-FLT16_MAX), double(FLT16_MAX)))));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<float>())
        {
            outHypData = HypData(float(MathUtil::Clamp(double(number), double(-FLT_MAX), double(FLT_MAX))));
            return true;
        }

        if (typeInfo.id == TypeId::ForType<double>())
        {
            outHypData = HypData(double(number));
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

        HypData stringInstance;
        if (!stringHandler->CreateInstance(stringInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of string type");
            return false;
        }

        stringHandler->SetValue(stringInstance, *jsonValue.AsString());

        outHypData = std::move(stringInstance);

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
                outHypData = HypData(jsonValue.ToInt8());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int16>())
            {
                outHypData = HypData(jsonValue.ToInt16());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int32>())
            {
                outHypData = HypData(jsonValue.ToInt32());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int64>())
            {
                outHypData = HypData(jsonValue.ToInt64());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint8>())
            {
                outHypData = HypData(jsonValue.ToUInt8());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint16>())
            {
                outHypData = HypData(jsonValue.ToUInt16());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint32>())
            {
                outHypData = HypData(jsonValue.ToUInt32());
                return true;
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint64>())
            {
                outHypData = HypData(jsonValue.ToUInt64());
                return true;
            }
            else
            {
                HYP_LOG(Core, Warning, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);
                return false;
            }
        }

        // @TODO String representation of enum values

        return false;
    }

    if (typeInfo.id == TypeId::ForType<Uuid>())
    {
        if (!jsonValue.IsString())
        {
            HYP_LOG(Core, Warning, "Expected JSON string for UUID, but got value: {}", jsonValue.ToString());
            return false;
        }

        const json::JSONString& jsonString = jsonValue.AsString();

        if (jsonString.Size() != 36)
        {
            HYP_LOG(Core, Warning, "Invalid UUID string length: {}", jsonString.Size());
            return false;
        }

        outHypData = HypData(Uuid(*jsonString));
        return true;
    }

    if (typeInfo.id == TypeId::ForType<Name>())
    {
        outHypData = HypData(Name(CreateNameFromDynamicString(*jsonValue.ToString())));
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

        HypData vectorInstance;
        if (!vectorHandler->CreateInstance(vectorInstance))
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

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
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize vector element at index {} for type {}", i, typeInfo.name);

                return false;
            }

            vectorHandler->SetComponent(vectorInstance, i, elementData);
        }

        outHypData = std::move(vectorInstance);

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

        HypData matrixInstance;
        if (!matrixHandler->CreateInstance(matrixInstance))
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

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
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize matrix element at index {} for type {}", i, typeInfo.name);

                return false;
            }

            matrixHandler->SetElement(matrixInstance, i / matrixHandler->GetNumColumns(), i % matrixHandler->GetNumColumns(), elementData);
        }

        outHypData = std::move(matrixInstance);

        return true;
    }

    if (typeInfo.IsArrayType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for array type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());

            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Array type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* arrayHandler = static_cast<ITypeInfoArrayHandler*>(handler);

        HypData arrayInstance;
        if (!arrayHandler->CreateInstance(arrayInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of array type {}", typeInfo.name);

            return false;
        }

        arrayHandler->Resize(arrayInstance, jsonArray.Size());

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
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

        outHypData = std::move(arrayInstance);

        return true;
    }

    if (typeInfo.IsSetType())
    {
        if (!jsonValue.IsArray())
        {
            HYP_LOG(Core, Warning, "Expected JSON array for set type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());

            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            HYP_LOG(Core, Warning, "Set type {} does not have a valid element type", typeInfo.name);

            return false;
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* setHandler = static_cast<ITypeInfoSetHandler*>(handler);

        HypData setInstance;
        if (!setHandler->CreateInstance(setInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of set type {}", typeInfo.name);

            return false;
        }

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
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

        outHypData = std::move(setInstance);

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
            const Class* derivedClass = GetClass(*classValue.AsString());

            if (derivedClass)
            {
                if (instanceClass && !derivedClass->IsDerivedFrom(instanceClass))
                {
                    HYP_LOG(Core, Warning, "Class '{}' is not derived from expected class '{}'", derivedClass->GetName(), instanceClass->GetName());
                }
            }
            else
            {
                HYP_LOG(Core, Warning, "Class '{}' not found!", *classValue.AsString());
            }

            instanceClass = derivedClass;
        }

        if (instanceClass)
        {
            HypData instance;

            if (!instanceClass->CreateInstance(instance))
            {
                HYP_LOG(Core, Warning, "Failed to create instance of Class \"{}\"", instanceClass->GetName());
                return false;
            }

            if (!JSONToObject(jsonValue.AsObject(), typeInfoClass, instance))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize instance of \"{}\" from JSON", instanceClass->GetName());
                return false;
            }

            outHypData = std::move(instance);

            return true;
        }

        HYP_LOG(Core, Warning, "Could not find Class for type: {}", typeInfo.name);

        return false;
    }

    HYP_LOG(Core, Warning, "Failed to deserialize JSON to HypData of type: {}", typeInfo.name);

    return false;
}

} // namespace hyperion
