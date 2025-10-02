/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypDataJSONHelpers.hpp>

#include <core/json/JSON.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypData.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

bool ObjectToJSON(const HypClass* hypClass, const HypData& target, json::JSONObject& outJson, bool skipTransientProperties)
{
    HashSet<Name> usedMembers;

    while (hypClass != nullptr)
    {
        for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY, /* deep */ false))
        {
            if (skipTransientProperties)
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

                json::JSONValue jsonValue;

                if (!HypDataToJSON(property->Get(target), jsonValue, skipTransientProperties))
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

                json::JSONValue jsonValue;

                if (!HypDataToJSON(field->Get(target), jsonValue, skipTransientProperties))
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

                json::JSONValue jsonValue;

                if (!HypDataToJSON(constant->Get(), jsonValue, skipTransientProperties))
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

            const TypeId typeId = property.Get(target).ToRef().GetTypeId();

            HypData hypData;

            if (!JSONToHypData(value, typeId, hypData))
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

            const TypeId typeId = field.Get(target).ToRef().GetTypeId();

            HypData hypData;

            if (!JSONToHypData(value, typeId, hypData))
            {
                HYP_LOG(Core, Warning, "Failed to deserialize field \"{}\" of HypClass \"{}\" from json (TypeId: {})",
                    member.GetName(), hypClass->GetName(), typeId.Value());

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

// Macro to reduce repetition for scalar types
#define HYP_JSON_TO_HYPDATA_SCALAR(Type, ConversionFunc)        \
    if (typeId == TypeId::ForType<Type>())                      \
    {                                                           \
        outHypData = HypData(Type(jsonValue.ConversionFunc())); \
        return true;                                            \
    }

// Macro to reduce repetition for vector types
#define HYP_JSON_TO_HYPDATA_VECTOR(VecType, ElementConversionFunc) \
    if (typeId == TypeId::ForType<VecType>())                      \
    {                                                              \
        if (!jsonValue.IsArray())                                  \
        {                                                          \
            return false;                                          \
        }                                                          \
        VecType vec;                                               \
        const json::JSONArray& jsonArray = jsonValue.AsArray();    \
        if (jsonArray.Size() != HYP_ARRAY_SIZE(vec.values))        \
        {                                                          \
            return false;                                          \
        }                                                          \
        for (SizeType i = 0; i < HYP_ARRAY_SIZE(vec.values); i++)  \
        {                                                          \
            vec[i] = jsonArray[i].ElementConversionFunc();         \
        }                                                          \
        outHypData = HypData(vec);                                 \
        return true;                                               \
    }

// Generic helper to load JSON array into a container type
// This macro generates the deserialization code for ContainerType<ElementType>
#define HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, ElementType)                       \
    if (typeId == TypeId::ForType<ContainerType<ElementType>>())                           \
    {                                                                                      \
        ContainerType<ElementType> result;                                                 \
        for (SizeType i = 0; i < jsonArray.Size(); i++)                                    \
        {                                                                                  \
            HypData elementData;                                                           \
            if (!JSONToHypData(jsonArray[i], TypeId::ForType<ElementType>(), elementData)) \
            {                                                                              \
                return false;                                                              \
            }                                                                              \
            result.Add(elementData.Get<ElementType>());                                    \
        }                                                                                  \
        outHypData = HypData(std::move(result));                                           \
        return true;                                                                       \
    }

bool JSONToHypData(const json::JSONValue& jsonValue, TypeId typeId, HypData& outHypData)
{
    // Scalar types
    HYP_JSON_TO_HYPDATA_SCALAR(int8, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(int16, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(int32, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(int64, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(uint8, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(uint16, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(uint32, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(uint64, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(float, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(double, ToNumber)
    HYP_JSON_TO_HYPDATA_SCALAR(bool, ToBool)

    if (typeId == TypeId::ForType<String>())
    {
        outHypData = HypData(jsonValue.ToString());
        return true;
    }

    // Vector types
    HYP_JSON_TO_HYPDATA_VECTOR(Vec2i, ToInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec3i, ToInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec4i, ToInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec2u, ToUInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec3u, ToUInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec4u, ToUInt32)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec2f, ToFloat)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec3f, ToFloat)
    HYP_JSON_TO_HYPDATA_VECTOR(Vec4f, ToFloat)

    if (typeId == TypeId::ForType<Uuid>())
    {
        if (!jsonValue.IsString())
        {
            return false;
        }

        const json::JSONString& jsonString = jsonValue.AsString();

        if (jsonString.Size() != 36)
        {
            return false;
        }

        outHypData = HypData(Uuid(*jsonString));
        return true;
    }

    if (typeId == TypeId::ForType<Name>())
    {
        outHypData = HypData(Name(CreateNameFromDynamicString(*jsonValue.ToString())));
        return true;
    }

    // Check for array types
    if (jsonValue.IsArray())
    {
        const json::JSONArray& jsonArray = jsonValue.AsArray();

#define HYP_TRY_CONTAINER_TYPE(ContainerType)               \
    /* Primitive types */                                   \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, int8)   \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, int16)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, int32)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, int64)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, uint8)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, uint16) \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, uint32) \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, uint64) \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, float)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, double) \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, bool)   \
    /* String types */                                      \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, String) \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Name)   \
    /* Vector types */                                      \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec2i)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec3i)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec4i)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec2u)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec3u)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec4u)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec2f)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec3f)  \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Vec4f)  \
    /* Other types */                                       \
    HYP_JSON_ARRAY_TO_CONTAINER_IMPL(ContainerType, Uuid)

        HYP_TRY_CONTAINER_TYPE(Array)
        HYP_TRY_CONTAINER_TYPE(SortedArray)
        HYP_TRY_CONTAINER_TYPE(LinkedList)
        HYP_TRY_CONTAINER_TYPE(Stack)
        HYP_TRY_CONTAINER_TYPE(Queue)
        HYP_TRY_CONTAINER_TYPE(HashSet)
        HYP_TRY_CONTAINER_TYPE(FlatSet)

#undef HYP_TRY_CONTAINER_TYPE

        if (jsonArray.Size() > 0)
        {
            // We need to know the element type to proceed
            // This is a limitation - we can't infer the element type from JSON alone
            // The caller must provide the correct TypeId
            HYP_LOG(Core, Warning, "Failed to deserialize JSON array - unknown element type for TypeId: {}", typeId.Value());
        }

        return false;
    }

    // Object types
    if (jsonValue.IsObject())
    {
        const HypClass* hypClass = GetClass(typeId);

        if (hypClass)
        {
            HypData propertyValueHypData;

            if (!hypClass->CreateInstance(propertyValueHypData))
            {
                return false;
            }

            if (!JSONToObject(jsonValue.AsObject(), hypClass, propertyValueHypData))
            {
                return false;
            }

            outHypData = std::move(propertyValueHypData);
            return true;
        }
    }

    return false;
}

#undef HYP_JSON_TO_HYPDATA_SCALAR
#undef HYP_JSON_TO_HYPDATA_VECTOR

bool HypDataToJSON(const HypData& value, json::JSONValue& outJson, bool skipTransientProperties)
{
    if (value.IsNull())
    {
        outJson = json::JSONNull();

        return true;
    }

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

    if (value.IsArray())
    {
        HypDataArray& array = value.Get<HypDataArray>();

        if (!array.CanGetElementByIndex())
        {
            return false;
        }

        const SizeType size = array.Size();

        json::JSONArray jsonArray;
        jsonArray.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            json::JSONValue jsonValue;

            HypData element;
            if (!array.ElementAt(i, element))
            {
                return false;
            }

            if (!HypDataToJSON(element, jsonValue, skipTransientProperties))
            {
                return false;
            }

            jsonArray.PushBack(std::move(jsonValue));
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

    if (value.IsArray())
    {
        HypDataArray& array = value.Get<HypDataArray>();

        if (!array.CanGetElementByIndex())
        {
            return false;
        }

        const SizeType size = array.Size();

        json::JSONArray jsonArray;
        jsonArray.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            json::JSONValue jsonValue;

            HypData element;
            if (!array.ElementAt(i, element))
            {
                return false;
            }

            if (!HypDataToJSON(element, jsonValue, skipTransientProperties))
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

        if (!ObjectToJSON(hypClass, value, jsonObject, skipTransientProperties))
        {
            return false;
        }

        outJson = std::move(jsonObject);

        return true;
    }

    return false;
}

} // namespace hyperion
