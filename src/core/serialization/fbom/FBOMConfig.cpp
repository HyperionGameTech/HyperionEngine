/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMConfig.hpp>

#include <core/json/JSON.hpp>

namespace Hyperion::serialization {

#pragma region FBOMWriterConfig

void FBOMWriterConfig::SaveToJSON(Json::Value& outJson) const
{
    Json::JSObject object = {
        { "enableStaticData", enableStaticData },
        { "compressStaticData", compressStaticData }
    };

    outJson = object;
}

bool FBOMWriterConfig::LoadFromJSON(const Json::Value& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    Json::JSObject object = json.AsObject();

    enableStaticData = object["enableStaticData"].ToBool();
    compressStaticData = object["compressStaticData"].ToBool();

    return true;
}

#pragma endregion FBOMWriterConfig

#pragma region FBOMReaderConfig

void FBOMReaderConfig::SaveToJSON(Json::Value& outJson) const
{
    Json::JSObject object = {
        { "continueOnExternalLoadError", continueOnExternalLoadError },
        { "basePath", basePath }
    };

    outJson = object;
}

bool FBOMReaderConfig::LoadFromJSON(const Json::Value& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    Json::JSObject object = json.AsObject();

    continueOnExternalLoadError = object[u"continueOnExternalLoadError"].ToBool();
    basePath = object[u"basePath"].ToString();

    return true;
}

#pragma endregion FBOMReaderConfig

} // namespace Hyperion::serialization
