#include "ScriptLoader.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

LoadedAsset ScriptLoader::LoadAsset(LoaderState &state) const
{
    SourceFile source_file(state.filepath, state.stream.Max());
    state.stream.Read(source_file.GetBuffer(), source_file.GetSize());

    auto script = UniquePtr<Handle<Script>>::Construct(
        Engine::Get()->CreateObject<Script>(source_file)
    );

    return { { LoaderResult::Status::OK }, script.Cast<void>() };
}

} // namespace hyperion::v2
