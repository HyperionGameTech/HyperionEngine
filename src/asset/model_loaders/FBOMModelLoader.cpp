/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <FBOMModelLoader.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

AssetLoadResult FBOMModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    FBOMReader reader { FBOMReaderConfig {} };

    BoxedValue result;

    if (FBOMResult err = reader.LoadFromFile(state.filepath, result))
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to read serialized object: {}", err.message);
    }
    return LoadedAsset { std::move(result) };
}

} // namespace Hyperion
