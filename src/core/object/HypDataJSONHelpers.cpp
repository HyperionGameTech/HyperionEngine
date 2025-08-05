/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypDataJSONHelpers.hpp>

#include <core/json/JSON.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

bool ObjectToJSON(const HypClass* hypClass, const HypData& target, json::JSONObject& outJson)
{
    for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY))
    {
        if (const HypClassAttributeValue& attribute = member.GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
        {
            continue;
        }

        switch (member.GetMemberType())
        {
        case HypMemberType::TYPE_PROPERTY:
        {
            const HypProperty* property = static_cast<const HypProperty*>(&member);

            json::JSONValue jsonValue;

            if (!HypDataToJSON(property->Get(target), jsonValue))
            {
                return false;
            }

            String path = StringUtil::ToCamelCase(*property->GetName(), true);

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

            // skip fields that act as synthetic properties - they will be included twice otherwise
            if (field->GetAttribute("property").IsValid())
            {
                continue;
            }

            json::JSONValue jsonValue;

            if (!HypDataToJSON(field->Get(target), jsonValue))
            {
                return false;
            }

            String path = StringUtil::ToCamelCase(*field->GetName(), true);

            if (const HypClassAttributeValue& pathAttribute = field->GetAttribute("jsonpath"); pathAttribute.IsValid())
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
        case HypMemberType::TYPE_CONSTANT:
        {
            const HypConstant* constant = static_cast<const HypConstant*>(&member);

            // skip fields that act as synthetic properties - they will be included twice otherwise
            if (constant->GetAttribute("property").IsValid())
            {
                continue;
            }

            json::JSONValue jsonValue;

            if (!HypDataToJSON(constant->Get(), jsonValue))
            {
                return false;
            }

            String path = StringUtil::ToCamelCase(*constant->GetName(), true);

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

    return true;
}

bool JSONToObject(const json::JSONObject& jsonObject, const HypClass* hypClass, HypData& target)
{
    Array<const IHypMember*> membersToResolve;

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
                HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    member.GetName(), hypClass->GetName());

                return false;
            }

            property.Set(target, hypData);

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField& field = static_cast<const HypField&>(member);

            // skip fields that act as synthetic properties - they will be included twice otherwise
            if (field.GetAttribute("property").IsValid())
            {
                break;
            }

            const TypeId typeId = field.Get(target).ToRef().GetTypeId();

            HypData hypData;

            if (!JSONToHypData(value, typeId, hypData))
            {
                HYP_LOG(Config, Warning, "Failed to deserialize field \"{}\" of HypClass \"{}\" from json (TypeId: {})",
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

    for (const IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_FIELD | HypMemberType::TYPE_PROPERTY))
    {
        if (const HypClassAttributeValue& attribute = member.GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
        {
            continue;
        }

        if (const HypClassAttributeValue& pathAttribute = member.GetAttribute("jsonpath"); pathAttribute.IsValid())
        {
            const String path = pathAttribute.GetString();

            HYP_LOG(Config, Debug, "Deserializing JSON property \"{}\" for HypClass \"{}\"", path, hypClass->GetName());

            auto value = jsonObjectValue.Get(path);

            if (!value.value)
            {
                HYP_LOG(Config, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", path, hypClass->GetName());

                continue;
            }

            if (!resolveMember(member, value.Get()))
            {
                return false;
            }

            continue;
        }

        // try to resolve the member by name
        auto value = jsonObjectValue.Get(StringUtil::ToCamelCase(*member.GetName(), true));

        if (!value.value)
        {
            HYP_LOG(Config, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", member.GetName(), hypClass->GetName());

            continue;
        }

        if (!resolveMember(member, value.Get()))
        {
            return false;
        }
    }

    return true;
}

bool JSONToHypData(const json::JSONValue& jsonValue, TypeId typeId, HypData& outHypData)
{
    if (typeId == TypeId::ForType<int8>())
    {
        outHypData = HypData(int8(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<int16>())
    {
        outHypData = HypData(int16(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<int32>())
    {
        outHypData = HypData(int32(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<int64>())
    {
        outHypData = HypData(int64(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<uint8>())
    {
        outHypData = HypData(uint8(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<uint16>())
    {
        outHypData = HypData(uint16(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<uint32>())
    {
        outHypData = HypData(uint32(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<uint64>())
    {
        outHypData = HypData(uint64(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<float>())
    {
        outHypData = HypData(float(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<double>())
    {
        outHypData = HypData(double(jsonValue.ToNumber()));

        return true;
    }
    else if (typeId == TypeId::ForType<bool>())
    {
        outHypData = HypData(jsonValue.ToBool());

        return true;
    }
    else if (typeId == TypeId::ForType<String>())
    {
        outHypData = HypData(jsonValue.ToString());

        return true;
    }
    else if (typeId == TypeId::ForType<Vec2i>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 2)
        {
            return false;
        }

        outHypData = HypData(Vec2i(jsonArray[0].ToInt32(), jsonArray[1].ToInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec3i>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 3)
        {
            return false;
        }

        outHypData = HypData(Vec3i(jsonArray[0].ToInt32(), jsonArray[1].ToInt32(), jsonArray[2].ToInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec4i>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 4)
        {
            return false;
        }

        outHypData = HypData(Vec4i(jsonArray[0].ToInt32(), jsonArray[1].ToInt32(), jsonArray[2].ToInt32(), jsonArray[3].ToInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec2u>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 2)
        {
            return false;
        }

        outHypData = HypData(Vec2u(jsonArray[0].ToUInt32(), jsonArray[1].ToUInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec3u>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 3)
        {
            return false;
        }

        outHypData = HypData(Vec3u(jsonArray[0].ToUInt32(), jsonArray[1].ToUInt32(), jsonArray[2].ToUInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec4u>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 4)
        {
            return false;
        }

        outHypData = HypData(Vec4u(jsonArray[0].ToUInt32(), jsonArray[1].ToUInt32(), jsonArray[2].ToUInt32(), jsonArray[3].ToUInt32()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec2f>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 2)
        {
            return false;
        }

        outHypData = HypData(Vec2f(jsonArray[0].ToFloat(), jsonArray[1].ToFloat()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec3f>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 3)
        {
            return false;
        }

        outHypData = HypData(Vec3f(jsonArray[0].ToFloat(), jsonArray[1].ToFloat(), jsonArray[2].ToFloat()));

        return true;
    }
    else if (typeId == TypeId::ForType<Vec4f>())
    {
        if (!jsonValue.IsArray())
        {
            return false;
        }

        const json::JSONArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 4)
        {
            return false;
        }

        outHypData = HypData(Vec4f(jsonArray[0].ToFloat(), jsonArray[1].ToFloat(), jsonArray[2].ToFloat(), jsonArray[3].ToFloat()));

        return true;
    }
    else if (typeId == TypeId::ForType<UUID>())
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

        outHypData = HypData(UUID(ANSIStringView(*jsonString)));

        return true;
    }
    else if (typeId == TypeId::ForType<Name>())
    {
        outHypData = HypData(Name(CreateNameFromDynamicString(*jsonValue.ToString())));

        return true;
    }
    else
    {
        if (!jsonValue.IsObject())
        {
            return false;
        }

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

bool HypDataToJSON(const HypData& value, json::JSONValue& outJson)
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

    if (value.Is<UUID>())
    {
        const UUID& uuid = value.Get<UUID>();

        outJson = json::JSONString(uuid.ToString());

        return true;
    }

    if (value.Is<Name>())
    {
        const Name name = value.Get<Name>();

        outJson = json::JSONString(name.LookupString());

        return true;
    }

    const HypClass* hypClass = GetClass(value.GetTypeId());

    if (hypClass)
    {
        json::JSONObject jsonObject;

        if (!ObjectToJSON(hypClass, value, jsonObject))
        {
            return false;
        }

        outJson = std::move(jsonObject);

        return true;
    }

    return false;
}

} // namespace hyperion
