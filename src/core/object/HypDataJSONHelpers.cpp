/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypDataJSONHelpers.hpp>

#include <core/json/JSON.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>

#include <core/algorithm/FindIf.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

bool ObjectToJSON(const HypClass* hyp_class, const HypData& target, json::JSONObject& out_json)
{
    for (const IHypMember& member : hyp_class->GetMembers())
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

            json::JSONValue json_value;

            if (!HypDataToJSON(property->Get(target), json_value))
            {
                return false;
            }

            String path = property->GetName().LookupString();

            if (const HypClassAttributeValue& path_attribute = property->GetAttribute("jsonpath"); path_attribute.IsValid())
            {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);

                out_json = std::move(temp).AsObject();
            }
            else
            {
                out_json[path] = std::move(json_value);
            }

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField* field = static_cast<const HypField*>(&member);

            json::JSONValue json_value;

            if (!HypDataToJSON(field->Get(target), json_value))
            {
                return false;
            }
            String path = field->GetName().LookupString();

            if (const HypClassAttributeValue& path_attribute = field->GetAttribute("jsonpath"); path_attribute.IsValid())
            {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);

                out_json = std::move(temp).AsObject();
            }
            else
            {
                out_json[path] = std::move(json_value);
            }

            break;
        }
        case HypMemberType::TYPE_CONSTANT:
        {
            const HypConstant* constant = static_cast<const HypConstant*>(&member);

            json::JSONValue json_value;

            if (!HypDataToJSON(constant->Get(), json_value))
            {
                return false;
            }

            String path = constant->GetName().LookupString();

            if (const HypClassAttributeValue& path_attribute = constant->GetAttribute("jsonpath"); path_attribute.IsValid())
            {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);

                out_json = std::move(temp).AsObject();
            }
            else
            {
                out_json[path] = std::move(json_value);
            }

            break;
        }
        default:
            break;
        }
    }

    return true;
}

bool JSONToObject(const json::JSONObject& json_object, const HypClass* hyp_class, HypData& target)
{
    Array<const IHypMember*> members_to_resolve;

    auto resolve_member = [hyp_class, &target](const IHypMember& member, const json::JSONValue& value) -> bool
    {
        switch (member.GetMemberType())
        {
        case HypMemberType::TYPE_PROPERTY:
        {
            const HypProperty& property = static_cast<const HypProperty&>(member);

            const TypeId type_id = property.Get(target).ToRef().GetTypeId();

            HypData hyp_data;

            if (!JSONToHypData(value, type_id, hyp_data))
            {
                HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    member.GetName(), hyp_class->GetName());

                return false;
            }

            property.Set(target, hyp_data);

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField& field = static_cast<const HypField&>(member);

            const TypeId type_id = field.Get(target).ToRef().GetTypeId();

            HypData hyp_data;

            if (!JSONToHypData(value, type_id, hyp_data))
            {
                HYP_LOG(Config, Warning, "Failed to deserialize field \"{}\" of HypClass \"{}\" from json",
                    member.GetName(), hyp_class->GetName());

                return false;
            }

            field.Set(target, hyp_data);

            break;
        }
        default:
            break;
        }

        return true;
    };

    json::JSONValue json_object_value(json_object);

    for (const IHypMember& member : hyp_class->GetMembers())
    {
        if (const HypClassAttributeValue& attribute = member.GetAttribute("jsonignore"); attribute.IsValid() && attribute.GetBool())
        {
            continue;
        }

        if (const HypClassAttributeValue& path_attribute = member.GetAttribute("jsonpath"); path_attribute.IsValid())
        {
            const String path = path_attribute.GetString();

            HYP_LOG(Config, Debug, "Deserializing JSON property \"{}\" for HypClass \"{}\"", path, hyp_class->GetName());

            auto value = json_object_value.Get(path);

            if (!value.value)
            {
                HYP_LOG(Config, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", path, hyp_class->GetName());

                continue;
            }

            if (!resolve_member(member, value.Get()))
            {
                HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    path, hyp_class->GetName());

                return false;
            }

            continue;
        }

        // try to resolve the member by name
        auto value = json_object_value.Get(member.GetName().LookupString());

        if (!value.value)
        {
            HYP_LOG(Config, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", member.GetName(), hyp_class->GetName());

            continue;
        }

        if (!resolve_member(member, value.Get()))
        {
            HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                member.GetName(), hyp_class->GetName());

            return false;
        }
    }

    return true;
}

bool JSONToHypData(const json::JSONValue& json_value, TypeId type_id, HypData& out_hyp_data)
{
    if (type_id == TypeId::ForType<int8>())
    {
        out_hyp_data = HypData(int8(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<int16>())
    {
        out_hyp_data = HypData(int16(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<int32>())
    {
        out_hyp_data = HypData(int32(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<int64>())
    {
        out_hyp_data = HypData(int64(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<uint8>())
    {
        out_hyp_data = HypData(uint8(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<uint16>())
    {
        out_hyp_data = HypData(uint16(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<uint32>())
    {
        out_hyp_data = HypData(uint32(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<uint64>())
    {
        out_hyp_data = HypData(uint64(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<float>())
    {
        out_hyp_data = HypData(float(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<double>())
    {
        out_hyp_data = HypData(double(json_value.ToNumber()));

        return true;
    }
    else if (type_id == TypeId::ForType<bool>())
    {
        out_hyp_data = HypData(json_value.ToBool());

        return true;
    }
    else if (type_id == TypeId::ForType<String>())
    {
        out_hyp_data = HypData(json_value.ToString());

        return true;
    }
    else if (type_id == TypeId::ForType<Vec2i>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 2)
        {
            return false;
        }

        out_hyp_data = HypData(Vec2i(json_array[0].ToInt32(), json_array[1].ToInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec3i>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 3)
        {
            return false;
        }

        out_hyp_data = HypData(Vec3i(json_array[0].ToInt32(), json_array[1].ToInt32(), json_array[2].ToInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec4i>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 4)
        {
            return false;
        }

        out_hyp_data = HypData(Vec4i(json_array[0].ToInt32(), json_array[1].ToInt32(), json_array[2].ToInt32(), json_array[3].ToInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec2u>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 2)
        {
            return false;
        }

        out_hyp_data = HypData(Vec2u(json_array[0].ToUInt32(), json_array[1].ToUInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec3u>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 3)
        {
            return false;
        }

        out_hyp_data = HypData(Vec3u(json_array[0].ToUInt32(), json_array[1].ToUInt32(), json_array[2].ToUInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec4u>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 4)
        {
            return false;
        }

        out_hyp_data = HypData(Vec4u(json_array[0].ToUInt32(), json_array[1].ToUInt32(), json_array[2].ToUInt32(), json_array[3].ToUInt32()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec2f>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 2)
        {
            return false;
        }

        out_hyp_data = HypData(Vec2f(json_array[0].ToFloat(), json_array[1].ToFloat()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec3f>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 3)
        {
            return false;
        }

        out_hyp_data = HypData(Vec3f(json_array[0].ToFloat(), json_array[1].ToFloat(), json_array[2].ToFloat()));

        return true;
    }
    else if (type_id == TypeId::ForType<Vec4f>())
    {
        if (!json_value.IsArray())
        {
            return false;
        }

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Size() != 4)
        {
            return false;
        }

        out_hyp_data = HypData(Vec4f(json_array[0].ToFloat(), json_array[1].ToFloat(), json_array[2].ToFloat(), json_array[3].ToFloat()));

        return true;
    }
    else
    {
        if (!json_value.IsObject())
        {
            return false;
        }

        const HypClass* hyp_class = GetClass(type_id);

        if (hyp_class)
        {
            HypData property_value_hyp_data;
            if (!hyp_class->CreateInstance(property_value_hyp_data))
            {
                return false;
            }

            if (!JSONToObject(json_value.AsObject(), hyp_class, property_value_hyp_data))
            {
                return false;
            }

            out_hyp_data = std::move(property_value_hyp_data);

            return true;
        }
    }

    return false;
}

bool HypDataToJSON(const HypData& value, json::JSONValue& out_json)
{
    if (value.IsNull())
    {
        out_json = json::JSONNull();

        return true;
    }

    if (value.Is<bool>(/* strict */ true))
    {
        out_json = json::JSONBool(value.Get<bool>());

        return true;
    }

    if (value.Is<double>(/* strict */ false))
    {
        out_json = json::JSONNumber(value.Get<double>());

        return true;
    }

    if (value.Is<String>())
    {
        out_json = json::JSONString(value.Get<String>());

        return true;
    }

    if (value.Is<Vec2i>())
    {
        const Vec2i& vec = value.Get<Vec2i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3i>())
    {
        const Vec3i& vec = value.Get<Vec3i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4i>())
    {
        const Vec4i& vec = value.Get<Vec4i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec2u>())
    {
        const Vec2u& vec = value.Get<Vec2u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3u>())
    {
        const Vec3u& vec = value.Get<Vec3u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4u>())
    {
        const Vec4u& vec = value.Get<Vec4u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec2f>())
    {
        const Vec2f& vec = value.Get<Vec2f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3f>())
    {
        const Vec3f& vec = value.Get<Vec3f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4f>())
    {
        const Vec4f& vec = value.Get<Vec4f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    const HypClass* hyp_class = GetClass(value.GetTypeId());

    if (hyp_class)
    {
        json::JSONObject json_object;

        if (!ObjectToJSON(hyp_class, value, json_object))
        {
            return false;
        }

        out_json = std::move(json_object);

        return true;
    }

    return false;
}

} // namespace hyperion