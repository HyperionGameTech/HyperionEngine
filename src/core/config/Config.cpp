/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/config/Config.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>

#include <core/algorithm/FindIf.hpp>

#include <core/utilities/Format.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {
namespace config {

static const ConfigurationValue g_invalid_configuration_value { };

#pragma region ConfigurationDataStore

ConfigurationDataStore::ConfigurationDataStore(UTF8StringView config_name)
    : DataStoreBase("config", DataStoreOptions { /* flags */ DSF_RW, /* max_size */ 0ull }),
      m_config_name(config_name)
{
}

ConfigurationDataStore::ConfigurationDataStore(ConfigurationDataStore &&other) noexcept
    : DataStoreBase(static_cast<DataStoreBase &&>(std::move(other))),
      m_config_name(std::move(other.m_config_name))
{
}

ConfigurationDataStore::~ConfigurationDataStore()
{
}

FilePath ConfigurationDataStore::GetFilePath() const
{
    FilePath config_path = GetDirectory() / m_config_name;

    if (!config_path.EndsWith(".json")) {
        config_path = config_path + ".json";
    }

    return config_path;
}

bool ConfigurationDataStore::Read(json::JSONValue &out_value) const
{
    const FilePath config_path = GetFilePath();
    
    if (!config_path.Exists()) {
        return false;
    }

    BufferedReader reader;

    if (!config_path.Open(reader)) {
        return false;
    }

    json::ParseResult parse_result = json::JSON::Parse(String(reader.ReadBytes().ToByteView()));

    if (!parse_result.ok) {
        HYP_LOG(Config, Warning, "Invalid JSON in configuration file at {}: {}", config_path, parse_result.message);

        return false;
    }

    out_value = std::move(parse_result.value);

    return true;
}

bool ConfigurationDataStore::Write(const json::JSONValue &value) const
{
    const String value_string = value.ToString(true);

    FileByteWriter writer(GetFilePath());
    writer.WriteString(value_string, BYTE_WRITER_FLAGS_NONE);
    writer.Close();
    
    return true;
}

#pragma endregion ConfigurationDataStore

#pragma region ConfigurationTable

ConfigurationTable::ConfigurationTable()
    : m_root_object(json::JSONObject()),
      m_data_store(nullptr)
{
}

ConfigurationTable::ConfigurationTable(const String &config_name, const String &subobject_path)
    : m_subobject_path(subobject_path.Any() ? subobject_path : Optional<String> { }),
      m_root_object(json::JSONObject()),
      m_data_store(&DataStoreBase::GetOrCreate<ConfigurationDataStore>(config_name)),
      m_data_store_resource_handle(*m_data_store)
{
    // try to read from config file
    if (!m_data_store->Read(m_root_object)) {
        HYP_LOG(Config, Warning, "Configuration could not be read: {}", m_data_store->GetFilePath());
    }

    m_cached_hash_code = GetSubobject().GetHashCode();
}

ConfigurationTable::ConfigurationTable(const String &config_name)
    : ConfigurationTable(config_name, String::empty)
{
}

ConfigurationTable::ConfigurationTable(const String &config_name, const HypClass *hyp_class)
    : ConfigurationTable(config_name, hyp_class ? hyp_class->GetAttribute("configpath").GetString() : String::empty)
{
}

ConfigurationTable::ConfigurationTable(const ConfigurationTable &other)
    : m_subobject_path(other.m_subobject_path),
      m_root_object(other.m_root_object),
      m_data_store(other.m_data_store),
      m_data_store_resource_handle(other.m_data_store_resource_handle),
      m_cached_hash_code(other.m_cached_hash_code)
{
}

ConfigurationTable &ConfigurationTable::operator=(const ConfigurationTable &other)
{
    if (this == &other) {
        return *this;
    }

    m_subobject_path = other.m_subobject_path;
    m_root_object = other.m_root_object;
    m_data_store = other.m_data_store;
    m_data_store_resource_handle = other.m_data_store_resource_handle;
    m_cached_hash_code = other.m_cached_hash_code;

    return *this;
}

ConfigurationTable::ConfigurationTable(ConfigurationTable &&other) noexcept
    : m_subobject_path(std::move(other.m_subobject_path)),
      m_root_object(std::move(other.m_root_object)),
      m_data_store(other.m_data_store),
      m_data_store_resource_handle(std::move(other.m_data_store_resource_handle)),
      m_cached_hash_code(std::move(other.m_cached_hash_code))
{
    other.m_data_store = nullptr;
}

ConfigurationTable &ConfigurationTable::operator=(ConfigurationTable &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_subobject_path = std::move(other.m_subobject_path);
    m_root_object = std::move(other.m_root_object);
    m_data_store = other.m_data_store;
    m_data_store_resource_handle = std::move(other.m_data_store_resource_handle);
    m_cached_hash_code = std::move(other.m_cached_hash_code);

    other.m_data_store = nullptr;

    return *this;
}

bool ConfigurationTable::IsChanged() const
{
    return GetSubobject().GetHashCode() != m_cached_hash_code;
}

ConfigurationTable &ConfigurationTable::Merge(const ConfigurationTable &other)
{
    if (this == &other) {
        return *this;
    }

    const json::JSONValue &other_subobject = other.GetSubobject();

    if (!other_subobject.IsObject()) {
        return *this;
    }

    json::JSONValue &target_object = other.m_subobject_path.HasValue()
        ? *m_root_object.Get(*other.m_subobject_path, /* create_intermediate_objects */ true)
        : m_root_object;

    if (!target_object.IsObject()) {
        target_object = json::JSONObject();
    }

    target_object.AsObject().Merge(other_subobject.AsObject());

    return *this;
}

const ConfigurationValue &ConfigurationTable::Get(UTF8StringView key) const
{
    auto select_result = GetSubobject().Get(key);

    if (select_result.value != nullptr) {
        return *select_result.value;
    }

    return g_invalid_configuration_value;
}

void ConfigurationTable::Set(UTF8StringView key, const ConfigurationValue &value)
{
    GetSubobject().Set(key, value);
}

bool ConfigurationTable::Save()
{
    AssertThrow(m_data_store != nullptr);

    if (m_data_store->Write(m_root_object)) {
        m_cached_hash_code = GetSubobject().GetHashCode();

        return true;
    }

    return false;
}

json::JSONValue &ConfigurationTable::GetSubobject()
{
    json::JSONValue *subobject = &m_root_object;

    if (m_subobject_path.HasValue()) {
        subobject = &*m_root_object.Get(*m_subobject_path, /* create_intermediate_objects */ true);

        if (!subobject->IsObject()) {
            *subobject = json::JSONObject();
        }
    }

    return *subobject;
}

const json::JSONValue &ConfigurationTable::GetSubobject() const
{
    const json::JSONValue *subobject = &m_root_object;

    if (m_subobject_path.HasValue()) {
        subobject = &*m_root_object.Get(*m_subobject_path);

        if (!subobject->IsObject()) {
            subobject = &json::JSONValue::s_empty_object;
        }
    }

    return *subobject;
}

static bool JSONToHypData(const json::JSONValue &json_value, TypeID type_id, HypData &out_hyp_data);
static bool HypDataToJSON(const HypData &value, json::JSONValue &out_json);

static bool ObjectToJSON(const HypClass *hyp_class, const HypData &target, json::JSONObject &out_json)
{
    for (const IHypMember &member : hyp_class->GetMembers()) {
        if (const HypClassAttributeValue &attribute = member.GetAttribute("configignore"); attribute.IsValid() && attribute.GetBool()) {
            continue;
        }

        switch (member.GetMemberType()) {
        case HypMemberType::TYPE_PROPERTY:
        {
            const HypProperty *property = static_cast<const HypProperty *>(&member);

            json::JSONValue json_value;

            if (!HypDataToJSON(property->Get(target), json_value)) {
                return false;
            }

            String path = property->GetName().LookupString();

            if (const HypClassAttributeValue &path_attribute = property->GetAttribute("configpath"); path_attribute.IsValid()) {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);

                out_json = std::move(temp).AsObject();
            } else {
                out_json[path] = std::move(json_value);
            }

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField *field = static_cast<const HypField *>(&member);

            json::JSONValue json_value;

            if (!HypDataToJSON(field->Get(target), json_value)) {
                return false;
            }
            String path = field->GetName().LookupString();

            if (const HypClassAttributeValue &path_attribute = field->GetAttribute("configpath"); path_attribute.IsValid()) {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);
                
                out_json = std::move(temp).AsObject();
            } else {
                out_json[path] = std::move(json_value);
            }

            break;
        }
        case HypMemberType::TYPE_CONSTANT:
        {
            const HypConstant *constant = static_cast<const HypConstant *>(&member);

            json::JSONValue json_value;

            if (!HypDataToJSON(constant->Get(), json_value)) {
                return false;
            }

            String path = constant->GetName().LookupString();

            if (const HypClassAttributeValue &path_attribute = constant->GetAttribute("configpath"); path_attribute.IsValid()) {
                path = path_attribute.GetString();

                json::JSONValue temp(std::move(out_json));
                temp.Set(path, json_value);

                out_json = std::move(temp).AsObject();
            } else {
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

static bool JSONToObject(const json::JSONObject &json_object, const HypClass *hyp_class, HypData &target)
{
    Array<const IHypMember *> members_to_resolve;

    auto ResolveMember = [hyp_class, &target](const IHypMember &member, const json::JSONValue &value) -> bool
    {
        switch (member.GetMemberType()) {
        case HypMemberType::TYPE_PROPERTY:
        {
            const HypProperty &property = static_cast<const HypProperty &>(member);

            const TypeID type_id = property.Get(target).ToRef().GetTypeID();

            HypData hyp_data;

            if (!JSONToHypData(value, type_id, hyp_data)) {
                HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    member.GetName(), hyp_class->GetName());

                return false;
            }

            property.Set(target, hyp_data);

            break;
        }
        case HypMemberType::TYPE_FIELD:
        {
            const HypField &field = static_cast<const HypField &>(member);

            const TypeID type_id = field.Get(target).ToRef().GetTypeID();

            HypData hyp_data;

            if (!JSONToHypData(value, type_id, hyp_data)) {
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

    for (const IHypMember &member : hyp_class->GetMembers()) {
        if (const HypClassAttributeValue &attribute = member.GetAttribute("configignore"); attribute.IsValid() && attribute.GetBool()) {
            continue;
        }

        const HypClassAttributeValue &path_attribute = member.GetAttribute("configpath");

        if (path_attribute.IsValid()) {
            const String path = path_attribute.GetString();

            HYP_LOG(Config, Debug, "Deserializing JSON property \"{}\" for HypClass \"{}\"", path, hyp_class->GetName());

            auto value = json_object_value.Get(path);

            if (!value.value) {
                HYP_LOG(Config, Warning, "Failed to resolve JSON property \"{}\" for HypClass \"{}\"", path, hyp_class->GetName());

                continue;
            }

            if (!ResolveMember(member, value.Get())) {
                HYP_LOG(Config, Warning, "Failed to deserialize property \"{}\" of HypClass \"{}\" from json",
                    path, hyp_class->GetName());

                return false;
            }
        }
    }

    return true;
}

static bool JSONToHypData(const json::JSONValue &json_value, TypeID type_id, HypData &out_hyp_data)
{
    if (type_id == TypeID::ForType<int8>()) {
        out_hyp_data = HypData(int8(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<int16>()) {
        out_hyp_data = HypData(int16(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<int32>()) {
        out_hyp_data = HypData(int32(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<int64>()) {
        out_hyp_data = HypData(int64(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<uint8>()) {
        out_hyp_data = HypData(uint8(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<uint16>()) {
        out_hyp_data = HypData(uint16(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<uint32>()) {
        out_hyp_data = HypData(uint32(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<uint64>()) {
        out_hyp_data = HypData(uint64(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<float>()) {
        out_hyp_data = HypData(float(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<double>()) {
        out_hyp_data = HypData(double(json_value.ToNumber()));

        return true;
    } else if (type_id == TypeID::ForType<bool>()) {
        out_hyp_data = HypData(json_value.ToBool());

        return true;
    } else if (type_id == TypeID::ForType<String>()) {
        out_hyp_data = HypData(json_value.ToString());

        return true;
    } else if (type_id == TypeID::ForType<Vec2i>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 2) {
            return false;
        }

        out_hyp_data = HypData(Vec2i(json_array[0].ToInt32(), json_array[1].ToInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec3i>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 3) {
            return false;
        }

        out_hyp_data = HypData(Vec3i(json_array[0].ToInt32(), json_array[1].ToInt32(), json_array[2].ToInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec4i>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 4) {
            return false;
        }

        out_hyp_data = HypData(Vec4i(json_array[0].ToInt32(), json_array[1].ToInt32(), json_array[2].ToInt32(), json_array[3].ToInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec2u>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 2) {
            return false;
        }

        out_hyp_data = HypData(Vec2u(json_array[0].ToUInt32(), json_array[1].ToUInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec3u>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 3) {
            return false;
        }

        out_hyp_data = HypData(Vec3u(json_array[0].ToUInt32(), json_array[1].ToUInt32(), json_array[2].ToUInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec4u>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 4) {
            return false;
        }

        out_hyp_data = HypData(Vec4u(json_array[0].ToUInt32(), json_array[1].ToUInt32(), json_array[2].ToUInt32(), json_array[3].ToUInt32()));

        return true;
    } else if (type_id == TypeID::ForType<Vec2f>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 2) {
            return false;
        }

        out_hyp_data = HypData(Vec2f(json_array[0].ToFloat(), json_array[1].ToFloat()));

        return true;
    } else if (type_id == TypeID::ForType<Vec3f>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 3) {
            return false;
        }

        out_hyp_data = HypData(Vec3f(json_array[0].ToFloat(), json_array[1].ToFloat(), json_array[2].ToFloat()));

        return true;
    } else if (type_id == TypeID::ForType<Vec4f>()) {
        if (!json_value.IsArray()) {
            return false;
        }

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Size() != 4) {
            return false;
        }

        out_hyp_data = HypData(Vec4f(json_array[0].ToFloat(), json_array[1].ToFloat(), json_array[2].ToFloat(), json_array[3].ToFloat()));

        return true;
    } else {
        if (!json_value.IsObject()) {
            return false;
        }

        const HypClass *hyp_class = GetClass(type_id);

        if (hyp_class) {
            HypData property_value_hyp_data;
            hyp_class->CreateInstance(property_value_hyp_data);

            if (!JSONToObject(json_value.AsObject(), hyp_class, property_value_hyp_data)) {
                return false;
            }

            out_hyp_data = std::move(property_value_hyp_data);

            return true;
        }
    }

    return false;
}

static bool HypDataToJSON(const HypData &value, json::JSONValue &out_json)
{
    if (value.IsNull()) {
        out_json = json::JSONNull();

        return true;
    }

    if (value.Is<bool>(/* strict */ true)) {
        out_json = json::JSONBool(value.Get<bool>());

        return true;
    }

    if (value.Is<double>(/* strict */ false)) {
        out_json = json::JSONNumber(value.Get<double>());

        return true;
    }

    if (value.Is<String>()) {
        out_json = json::JSONString(value.Get<String>());

        return true;
    }

    if (value.Is<Vec2i>()) {
        const Vec2i &vec = value.Get<Vec2i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3i>()) {
        const Vec3i &vec = value.Get<Vec3i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4i>()) {
        const Vec4i &vec = value.Get<Vec4i>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec2u>()) {
        const Vec2u &vec = value.Get<Vec2u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3u>()) {
        const Vec3u &vec = value.Get<Vec3u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4u>()) {
        const Vec4u &vec = value.Get<Vec4u>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec2f>()) {
        const Vec2f &vec = value.Get<Vec2f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec3f>()) {
        const Vec3f &vec = value.Get<Vec3f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));

        out_json = std::move(json_array);

        return true;
    }

    if (value.Is<Vec4f>()) {
        const Vec4f &vec = value.Get<Vec4f>();

        json::JSONArray json_array;
        json_array.PushBack(json::JSONNumber(vec.x));
        json_array.PushBack(json::JSONNumber(vec.y));
        json_array.PushBack(json::JSONNumber(vec.z));
        json_array.PushBack(json::JSONNumber(vec.w));

        out_json = std::move(json_array);

        return true;
    }

    const HypClass *hyp_class = GetClass(value.GetTypeID());

    if (hyp_class) {
        json::JSONObject json_object;

        if (!ObjectToJSON(hyp_class, value, json_object)) {
            return false;
        }

        out_json = std::move(json_object);

        return true;
    }

    return false;
}

const String &ConfigurationTable::GetDefaultConfigName(const HypClass *hyp_class)
{
    if (hyp_class) {
        if (const HypClassAttributeValue &config_name_attribute_value = hyp_class->GetAttribute("configname")) {
            return config_name_attribute_value.GetString();
        }
    }

    return String::empty;
}

void ConfigurationTable::AddError(const Error &error)
{
    m_errors.PushBack(error);
}

void ConfigurationTable::LogErrors() const
{
    if (m_errors.Empty()) {
        return;
    }

    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):",
        m_data_store ? m_data_store->GetConfigName() : "<unknown>",
        m_data_store ? m_data_store->GetFilePath() : "<unknown>");

    for (const Error &error : m_errors) {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }
}

void ConfigurationTable::LogErrors(UTF8StringView message) const
{
    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):",
        m_data_store ? m_data_store->GetConfigName() : "<unknown>",
        m_data_store ? m_data_store->GetFilePath() : "<unknown>");

    for (const Error &error : m_errors) {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }

    HYP_LOG(Config, Error, "{}", message);
}

bool ConfigurationTable::SetHypClassFields(const HypClass *hyp_class, const void *ptr)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(ptr != nullptr);

    AnyRef target_ref(hyp_class->GetTypeID(), const_cast<void *>(ptr));
    HypData target_hyp_data = HypData(target_ref);

    if (!JSONToObject(GetSubobject().AsObject(), hyp_class, target_hyp_data)) {
        HYP_LOG(Config, Error, "Failed to deserialize JSON to instance of HypClass \"{}\"", hyp_class->GetName());

        return false;
    }

    json::JSONObject json_object;

    if (ObjectToJSON(hyp_class, target_hyp_data, json_object)) {
        GetSubobject().AsObject() = std::move(json_object.Merge(GetSubobject().AsObject()));

        return true;
    } else {
        HYP_LOG(Config, Error, "Failed to serialize HypClass \"{}\" to json", hyp_class->GetName());

        return false;
    }
}


#pragma endregion ConfigurationTable

} // namespace config
} // namespace hyperion