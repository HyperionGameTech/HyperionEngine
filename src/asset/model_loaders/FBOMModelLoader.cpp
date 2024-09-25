/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/FBOMModelLoader.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

LoadedAsset FBOMModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    fbom::FBOMReader reader { fbom::FBOMReaderConfig { } };
    
    HypData result;

    HYP_LOG(Assets, LogLevel::DEBUG, "Begin loading serialized object at {}", state.filepath);

    if (fbom::FBOMResult err = reader.LoadFromFile(state.filepath, result)) {
        return { { LoaderResult::Status::ERR, err.message } };
    }
    return { { LoaderResult::Status::OK }, std::move(result) };
}

} // namespace hyperion
