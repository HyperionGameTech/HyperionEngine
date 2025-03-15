/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/data_loaders/JSONLoader.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion {

AssetLoadResult JSONLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);
    JSONValue json;

    const ByteBuffer byte_buffer = state.stream.ReadBytes();

    if (!byte_buffer.Size()) {
        return HYP_MAKE_ERROR(AssetLoadError, "Empty JSON file");
    }

    const String json_string(byte_buffer.ToByteView());

    const auto json_parse_result = JSON::Parse(json_string);

    if (!json_parse_result.ok) {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse json: {}", json_parse_result.message);
    }

    return LoadedAsset { json_parse_result.value };
}

} // namespace hyperion
