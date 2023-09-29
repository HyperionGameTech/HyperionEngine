#include "ScriptLoader.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

LoadedAsset ScriptLoader::LoadAsset(LoaderState &state) const
{
    SourceFile source_file(state.filepath, state.stream.Max());

    ByteBuffer temp_buffer;
    temp_buffer.SetSize(source_file.GetSize());

    state.stream.Read(temp_buffer);

    source_file.ReadIntoBuffer(temp_buffer);

    auto script = UniquePtr<Handle<Script>>::Construct(
        CreateObject<Script>(source_file)
    );

    return { { LoaderResult::Status::OK }, script.Cast<void>() };
}

} // namespace hyperion::v2
