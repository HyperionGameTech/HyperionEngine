#include <asset/data_loaders/JSONLoader.hpp>
#include <Engine.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

LoadedAsset JSONLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);
    JSONValue json;

    ByteBuffer byte_buffer = state.stream.ReadBytes();

    if (!byte_buffer.Size()) {
        return { { LoaderResult::Status::ERR_EOF } };
    }

    const String json_string(byte_buffer);

    const auto json_parse_result = JSON::Parse(json_string);

    if (!json_parse_result.ok) {
        return { { LoaderResult::Status::ERR, json_parse_result.message } };
    }

    return {
        { LoaderResult::Status::OK },
        UniquePtr<JSONValue>::Construct(json_parse_result.value).Cast<void>()
    };
}

} // namespace hyperion::v2
