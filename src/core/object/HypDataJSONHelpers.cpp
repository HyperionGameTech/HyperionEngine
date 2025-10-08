/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypDataJSONHelpers.hpp>

#include <core/json/JSON.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypData.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/TypeInfo.hpp>
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

// used to prevent infinite recursion when serializing nested objects
static thread_local HashSet<const void*> s_serializedObjects;

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

    if (!s_serializedObjects.Insert(value.ToRef().GetPointer()).second)
    {
        HYP_LOG(Core, Warning, "Detected circular reference when serializing HypData to JSON!");

        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    HYP_DEFER({
        s_serializedObjects.Erase(value.ToRef().GetPointer());
    });

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

    if (value.Is<double>(/* strict */ false))
    {
        outJson = json::JSONNumber(value.Get<double>());

        return true;
    }

    if (value.Is<String>())
    {
        outJson = json::JSONString(value.Get<String>());

        return true;
    }

    if (value.Is<Vec2i>())
    {
        const Vec2i& vec = value.Get<Vec2i>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec3i>())
    {
        const Vec3i& vec = value.Get<Vec3i>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec4i>())
    {
        const Vec4i& vec = value.Get<Vec4i>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));
        jsonArray.PushBack(json::JSONNumber(vec.w));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec2u>())
    {
        const Vec2u& vec = value.Get<Vec2u>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec3u>())
    {
        const Vec3u& vec = value.Get<Vec3u>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec4u>())
    {
        const Vec4u& vec = value.Get<Vec4u>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));
        jsonArray.PushBack(json::JSONNumber(vec.w));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec2f>())
    {
        const Vec2f& vec = value.Get<Vec2f>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec3f>())
    {
        const Vec3f& vec = value.Get<Vec3f>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));

        outJson = std::move(jsonArray);

        return true;
    }

    if (value.Is<Vec4f>())
    {
        const Vec4f& vec = value.Get<Vec4f>();

        json::JSONArray jsonArray;
        jsonArray.PushBack(json::JSONNumber(vec.x));
        jsonArray.PushBack(json::JSONNumber(vec.y));
        jsonArray.PushBack(json::JSONNumber(vec.z));
        jsonArray.PushBack(json::JSONNumber(vec.w));

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
        if (!typeInfo.extendedInfo.handler || typeInfo.extendedInfo.handler->GetHandlerType() != ITypeInfoHandler::TYPE_ARRAY)
        {
            HYP_LOG(Core, Warning, "TypeInfo for array type {} has invalid handler!", typeInfo.name);

            return false;
        }

        ITypeInfoArrayHandler* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);

        const SizeType size = handler->GetSize(value.ToRef());

        json::JSONArray jsonArray;
        jsonArray.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            AnyRef elementRef = handler->GetElementAt(value.ToRef(), i);
            
            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->id != elementRef.GetTypeId())
            {
                newOpts.writeClassNames = true;
            }
            
            json::JSONValue jsonValue;
            if (!HypDataToJSON(HypData(elementRef), jsonValue, newOpts))
            {
                return false;
            }

            jsonArray.PushBack(std::move(jsonValue));
        }

        outJson = std::move(jsonArray);

        return true;
    }

    const HypClass* hypClass = GetClass(value.GetTypeId());

    if (hypClass)
    {
        json::JSONObject jsonObject;

        GlobalContextScope contextScope { SaveAssetsAsReferencesContext() };;

        if (!ObjectToJSON(hypClass, value, jsonObject, opts))
        {
            return false;
        }

        if (opts.writeClassNames)
        {
            jsonObject["$Class"] = *hypClass->GetName();
        }

        outJson = std::move(jsonObject);

        return true;
    }

    HYP_LOG(Core, Warning, "Don't know how to serialize HypData with type \"{}\" to JSON", typeInfo.name);

    return false;
}

bool ObjectToJSON(
    const HypClass* hypClass,
    const HypData& target,
    json::JSONObject& outJson,
    ToJSONOptions opts)
{
    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    HashSet<Name> usedMembers;

    while (hypClass != nullptr)
    {
        // look for signature void ToJSON(JSONValue& out)
        if (const HypMethod* toJsonMethod = hypClass->GetMethod("ToJSON", /* deep */ false))
        {
            if (toJsonMethod->GetParameters().Size() != 2
                || toJsonMethod->GetTypeId() != TypeId::Void()
                || toJsonMethod->GetParameters()[0].typeInfo->id != hypClass->GetTypeId()
                || toJsonMethod->GetParameters()[1].typeInfo->id != TypeId::ForType<json::JSONValue>())
            {
                HYP_LOG(Core, Warning, "HypClass \"{}\" has a ToJSON method but it has an invalid signature", hypClass->GetName());
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
                        HYP_LOG(Core, Warning, "ToJSON method of HypClass \"{}\" did not return a valid JSON object, got JSON value: {}", hypClass->GetName(), jsonValue.ToString());
                    }
                }
                else
                {
                    HYP_LOG(Core, Warning, "Failed to invoke ToJSON method of HypClass \"{}\", got type {} but expected JSONValue!",
                        hypClass->GetName(), returnValue.GetTypeInfo()->name);
                }
            }
        }

        for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY, /* deep */ false))
        {
            if (opts.skipTransientProperties)
            {
                if (const HypClassAttributeValue& attribute = member.GetAttribute("transient"); attribute.IsValid() && attribute.GetBool())
                {
                    continue;
                }
            }

            if (const HypClassAttributeValue& attribute = member.GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
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

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && property->GetTypeId() != value.GetTypeId())
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize property \"{}\" of HypClass \"{}\" to json",
                        member.GetName(), hypClass->GetName());

                    continue;
                }

                String path = *property->GetName();

                if (const HypClassAttributeValue& pathAttribute = property->GetAttribute("jsonpath"); pathAttribute.IsValid())
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
                const HypField* field = static_cast<const HypField*>(&member);

                HypData value = field->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && field->GetTypeId() != value.GetTypeId())
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(value, jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize field \"{}\" of HypClass \"{}\" to json",
                        member.GetName(), hypClass->GetName());

                    continue;
                }

                String path = *field->GetName();

                if (const HypClassAttributeValue& pathAttribute = field->GetAttribute("jsonpath"); pathAttribute.IsValid())
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

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && constant->GetTypeId() != value.GetTypeId())
                {
                    newOpts.writeClassNames = true;
                }

                json::JSONValue jsonValue;

                if (!HypDataToJSON(constant->Get(), jsonValue, newOpts))
                {
                    HYP_LOG(Core, Warning, "Failed to serialize constant \"{}\" of HypClass \"{}\" to json",
                        member.GetName(), hypClass->GetName());

                    continue;
                }

                String path = *constant->GetName();

                if (const HypClassAttributeValue& pathAttribute = constant->GetAttribute("jsonpath"); pathAttribute.IsValid())
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

        hypClass = hypClass->GetParent();
    }

    return true;
}

bool JSONToObject(const json::JSONObject& jsonObject, const HypClass* hypClass, HypData& target)
{
    auto resolveMember = [hypClass, &target](const IHypMember& member, const json::JSONValue& value) -> bool
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
                HYP_LOG(Core, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    member.GetName(), hypClass->GetName());

                return false;
            }

            property.Set(target, hypData);

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField& field = static_cast<const HypField&>(member);
            const TypeInfo& typeInfo = field.GetTypeInfo();

            HypData hypData;

            if (!JSONToHypData(value, typeInfo, hypData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize field \"{}\" of HypClass \"{}\" from json (type: {})",
                    member.GetName(), hypClass->GetName(), typeInfo.name);

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

    json::JSONValue jsonObjectValue(jsonObject);

    // reoslve jsonpath members first
    for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY))
    {
        if (const HypClassAttributeValue& attribute = member.GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
        {
            continue;
        }

        const HypClassAttributeValue& pathAttribute = member.GetAttribute("jsonpath");

        if (!pathAttribute.IsValid())
        {
            continue;
        }

        const String& path = pathAttribute.GetString();

        auto value = jsonObjectValue.Get(path);

        if (!value.value)
        {
            HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for HypClass \"{}\"", path, hypClass->GetName());

            continue;
        }

        if (!resolveMember(member, value.Get()))
        {
            HYP_LOG(Core, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", path, hypClass->GetName());

            continue;
        }
    }

    // resolve data from json object to members by name
    for (const auto& [key, value] : jsonObject)
    {
        const IHypMember* member = hypClass->GetMember(key);

        if (!member)
        {
            // member not found, skip
            continue;
        }

        // skip members with jsonpath attribute - they were already resolved
        if (member->GetAttribute("jsonpath").IsValid())
        {
            continue;
        }

        // skip if jsonignore is set
        if (const HypClassAttributeValue& attribute = member->GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
        {
            continue;
        }

        if (!resolveMember(*member, value))
        {
            HYP_LOG(Core, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", key, hypClass->GetName());

            continue;
        }
    }

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

        Any stringInstance;
        if (!stringHandler->CreateInstance(stringInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of string type");
            return false;
        }

        stringHandler->SetValue(stringInstance.ToRef(), *jsonValue.AsString());

        return true;
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

        Any vectorInstance;
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

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize vector element at index {} for type {}", i, typeInfo.name);
                return false;
            }

            vectorHandler->SetComponent(vectorInstance.ToRef(), i, elementData.ToRef());
        }

        outHypData = HypData(std::move(vectorInstance));

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

        ITypeInfoHandler* handler = elementTypeInfo->extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_ARRAY)
        {
            HYP_LOG(Core, Warning, "Element type for array type {} does not have a valid array handler", typeInfo.name);
            return false;
        }

        ITypeInfoArrayHandler* arrayHandler = static_cast<ITypeInfoArrayHandler*>(handler);

        Any arrayInstance;
        if (!arrayHandler->CreateInstance(arrayInstance))
        {
            HYP_LOG(Core, Warning, "Failed to create instance of array type {}", typeInfo.name);
            return false;
        }

        arrayHandler->Resize(arrayInstance.ToRef(), jsonArray.Size());

        for (SizeType i = 0; i < jsonArray.Size(); i++)
        {
            HypData elementData;

            if (!JSONToHypData(jsonArray[i], *elementTypeInfo, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize array element at index {} for type: {}", i, typeInfo.name);
                return false;
            }

            arrayHandler->SetElementAt(arrayInstance.ToRef(), i, elementData.ToRef());
        }

        outHypData = HypData(std::move(arrayInstance));

        return true;
    }

    // Object types
    if (jsonValue.IsObject())
    {
        const HypClass* hypClass = GetClass(typeInfo.id);

        // first check for $Class property to override type
        if (auto classValue = jsonValue.Get("$Class"); classValue && classValue.IsString())
        {
            const HypClass* derivedClass = GetClass(*classValue.AsString());

            if (derivedClass)
            {
                if (hypClass && !derivedClass->IsDerivedFrom(hypClass))
                {
                    HYP_LOG(Core, Warning, "Class '{}' is not derived from expected class '{}'", derivedClass->GetName(), hypClass->GetName());
                }
            }
            else
            {
                HYP_LOG(Core, Warning, "Class '{}' not found!", *classValue.AsString());
            }

            hypClass = derivedClass;
        }

        if (hypClass)
        {
            HypData propertyValueHypData;

            if (!hypClass->CreateInstance(propertyValueHypData))
            {
                HYP_LOG(Core, Warning, "Failed to create instance of HypClass \"{}\"", hypClass->GetName());
                return false;
            }

            if (!JSONToObject(jsonValue.AsObject(), hypClass, propertyValueHypData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize HypClass \"{}\" from JSON", hypClass->GetName());
                return false;
            }

            outHypData = std::move(propertyValueHypData);
            return true;
        }
    }

    HYP_LOG(Core, Warning, "Failed to deserialize JSON to HypData of type: {}", typeInfo.name);

    return false;
}

} // namespace hyperion
