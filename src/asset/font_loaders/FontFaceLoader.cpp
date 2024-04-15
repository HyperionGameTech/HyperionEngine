/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/font_loaders/FontFaceLoader.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

LoadedAsset FontFaceLoader::LoadAsset(LoaderState &state) const
{
    FontEngine &font_engine = FontEngine::GetInstance();

    auto font_face = UniquePtr<FontFace>::Construct(
        font_engine.GetFontBackend(),
        state.filepath
    );

    return { { LoaderResult::Status::OK }, font_face.Cast<void>() };
}

} // namespace hyperion::v2
