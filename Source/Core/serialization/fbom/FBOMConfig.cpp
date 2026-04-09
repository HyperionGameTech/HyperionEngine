/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/serialization/fbom/FBOMConfig.hpp>

#include <Core/json/JSON.hpp>

namespace Hyperion::serialization {

#pragma region FBOMWriterConfig

void FBOMWriterConfig::SaveToJSON(JSON::Value& outJson) const
{
    JSON::Object object = {
        { "enableStaticData", enableStaticData },
        { "compressStaticData", compressStaticData }
    };

    outJson = object;
}

bool FBOMWriterConfig::LoadFromJSON(const JSON::Value& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    JSON::Object object = json.AsObject();

    enableStaticData = object["enableStaticData"].ToBool();
    compressStaticData = object["compressStaticData"].ToBool();

    return true;
}

#pragma endregion FBOMWriterConfig

#pragma region FBOMReaderConfig

void FBOMReaderConfig::SaveToJSON(JSON::Value& outJson) const
{
    JSON::Object object = {
        { "continueOnExternalLoadError", continueOnExternalLoadError },
        { "basePath", basePath }
    };

    outJson = object;
}

bool FBOMReaderConfig::LoadFromJSON(const JSON::Value& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    JSON::Object object = json.AsObject();

    continueOnExternalLoadError = object[u"continueOnExternalLoadError"].ToBool();
    basePath = object[u"basePath"].ToString();

    return true;
}

#pragma endregion FBOMReaderConfig

} // namespace Hyperion::serialization
