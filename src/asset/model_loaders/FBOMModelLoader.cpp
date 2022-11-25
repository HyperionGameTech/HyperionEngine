#include "FBOMModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

LoadedAsset FBOMModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    fbom::FBOMReader readerfbom::FBOMConfig { });
    fbom::FBOMDeserializedObject object;

    DebugLog(LogType::Info, "Begin loading serialized object at %s\n", state.filepath.c_str());

    if (auto err = reader.LoadFromFile(String(state.filepath.c_str()), object)) {
        return { { LoaderResult::Status::ERR, err.message } };
    }
    return { { LoaderResult::Status::OK }, std::move(object.m_value) };
}

} // namespace hyperion::v2
