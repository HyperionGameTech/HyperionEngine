/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/FontLoaders/FontFaceLoader.hpp>

#include <FontFaceLoader.generated.inl>

namespace Hyperion {

AssetLoadResult FontFaceLoader::LoadAsset(LoaderState& state) const
{
    FontEngine& fontEngine = FontEngine::GetInstance();

    RC<FontFace> fontFace = MakeRefCountedPtr<FontFace>(
        fontEngine.GetFontBackend(),
        state.filepath);

    return LoadedAsset { fontFace };
}

} // namespace Hyperion
