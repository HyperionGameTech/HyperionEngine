/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/FBOMModelLoader.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

AssetLoadResult FBOMModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    fbom::FBOMReader reader { fbom::FBOMReaderConfig { } };
    
    HypData result;

    HYP_LOG(Assets, Debug, "Begin loading serialized object at {}", state.filepath);

    if (fbom::FBOMResult err = reader.LoadFromFile(state.filepath, result)) {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to read serialized object: {}", err.message);
    }
    return LoadedAsset { std::move(result) };
}

} // namespace hyperion
