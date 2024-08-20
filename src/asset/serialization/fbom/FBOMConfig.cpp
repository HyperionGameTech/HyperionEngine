/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMConfig.hpp>

#include <util/json/JSON.hpp>

namespace hyperion::fbom {

#pragma region FBOMWriterConfig

void FBOMWriterConfig::SaveToJSON(json::JSONValue &out_json) const
{
    json::JSONObject object = {
        { "enable_static_data", enable_static_data },
        { "compress_static_data", compress_static_data }
    };

    out_json = object;
}

bool FBOMWriterConfig::LoadFromJSON(const json::JSONValue &json)
{
    if (!json.IsObject()) {
        return false;
    }

    json::JSONObject object = json.AsObject();

    enable_static_data = object["enable_static_data"].ToBool();
    compress_static_data = object["compress_static_data"].ToBool();

    return true;
}

#pragma endregion FBOMWriterConfig

#pragma region FBOMReaderConfig

void FBOMReaderConfig::SaveToJSON(json::JSONValue &out_json) const
{
    json::JSONObject object = {
        { "continue_on_external_load_error", continue_on_external_load_error },
        { "base_path", base_path }
    };

    // Don't save external_data_cache

    out_json = object;
}

bool FBOMReaderConfig::LoadFromJSON(const json::JSONValue &json)
{
    if (!json.IsObject()) {
        return false;
    }

    json::JSONObject object = json.AsObject();

    continue_on_external_load_error = object["continue_on_external_load_error"].ToBool();
    base_path = object["base_path"].ToString();

    return true;
}

#pragma endregion FBOMReaderConfig

} // namespace hyperion::fbom
