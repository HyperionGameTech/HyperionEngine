/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMConfig.hpp>

#include <core/json/JSON.hpp>

namespace hyperion::serialization {

#pragma region FBOMWriterConfig

void FBOMWriterConfig::SaveToJSON(json::JSONValue& outJson) const
{
    json::JSONObject object = {
        { "enableStaticData", enableStaticData },
        { "compressStaticData", compressStaticData }
    };

    outJson = object;
}

bool FBOMWriterConfig::LoadFromJSON(const json::JSONValue& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    json::JSONObject object = json.AsObject();

    enableStaticData = object["enableStaticData"].ToBool();
    compressStaticData = object["compressStaticData"].ToBool();

    return true;
}

#pragma endregion FBOMWriterConfig

#pragma region FBOMReaderConfig

void FBOMReaderConfig::SaveToJSON(json::JSONValue& outJson) const
{
    json::JSONObject object = {
        { "continueOnExternalLoadError", continueOnExternalLoadError },
        { "basePath", basePath }
    };

    outJson = object;
}

bool FBOMReaderConfig::LoadFromJSON(const json::JSONValue& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    json::JSONObject object = json.AsObject();

    continueOnExternalLoadError = object["continueOnExternalLoadError"].ToBool();
    basePath = object["basePath"].ToString();

    return true;
}

#pragma endregion FBOMReaderConfig

} // namespace hyperion::serialization
