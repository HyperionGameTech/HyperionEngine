/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/FBOMReader.hpp>

#include <Core/filesystem/FsUtil.hpp>

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
