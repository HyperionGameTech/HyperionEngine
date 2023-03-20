#include "FontLoader.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

LoadedAsset FontLoader::LoadAsset(LoaderState &state) const
{
    DebugLog(LogType::RenDebug, "Got to LoadAsset\n");
    FontEngine &font_engine = Engine::Get()->GetFontEngine();
    //Face face = font_engine.LoadFont(state.filepath);

    auto font_face = UniquePtr<Handle<Face>>::Construct(
        CreateObject<Face>(font_engine.GetFontBackend(), state.filepath)
    );

    return { { LoaderResult::Status::OK }, font_face.Cast<void>() };
}

} // namespace hyperion::v2
