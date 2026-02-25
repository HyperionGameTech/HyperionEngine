/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/font_loaders/FontFaceLoader.hpp>

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
