/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/font_loaders/FontFaceLoader.hpp>

namespace hyperion {

AssetLoadResult FontFaceLoader::LoadAsset(LoaderState& state) const
{
    FontEngine& fontEngine = FontEngine::GetInstance();

    RC<FontFace> fontFace = MakeRefCountedPtr<FontFace>(
        fontEngine.GetFontBackend(),
        state.filepath);

    return LoadedAsset { fontFace };
}

} // namespace hyperion
