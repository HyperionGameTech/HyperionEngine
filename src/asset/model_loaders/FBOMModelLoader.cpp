/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/FBOMModelLoader.hpp>

#include <rendering/RenderMaterial.hpp>

#include <scene/Mesh.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

LoadedAsset FBOMModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    fbom::FBOMReader reader { fbom::FBOMReaderConfig { } };
    
    HypData result;

    HYP_LOG(Assets, Debug, "Begin loading serialized object at {}", state.filepath);

    if (fbom::FBOMResult err = reader.LoadFromFile(state.filepath, result)) {
        return { { LoaderResult::Status::ERR, err.message } };
    }
    return { { LoaderResult::Status::OK }, std::move(result) };
}

} // namespace hyperion
