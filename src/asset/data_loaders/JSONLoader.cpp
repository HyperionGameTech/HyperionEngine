/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/data_loaders/JSONLoader.hpp>

#include <algorithm>
#include <stack>
#include <string>

namespace hyperion {

AssetLoadResult JSONLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);
    JSONValue json;

    const ByteBuffer byteBuffer = state.stream.ReadBytes();

    if (!byteBuffer.Size())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Empty JSON file");
    }

    const String jsonString(byteBuffer.ToByteView());

    const auto jsonParseResult = JSON::Parse(jsonString);

    if (!jsonParseResult.ok)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse json: {}", jsonParseResult.message);
    }

    return LoadedAsset { jsonParseResult.value };
}

} // namespace hyperion
