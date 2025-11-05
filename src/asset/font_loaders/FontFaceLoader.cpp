/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/font_loaders/FontFaceLoader.hpp>

#include <FontFaceLoader.generated.inl>

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
