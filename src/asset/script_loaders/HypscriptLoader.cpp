#include "HypscriptLoader.hpp"
#include <Engine.hpp>
#include <util/img/stb_image.h>

namespace hyperion::v2 {

using ScriptLoader = LoaderObject<Script, LoaderFormat::SCRIPT_HYPSCRIPT>::Loader;

LoaderResult ScriptLoader::LoadFn(LoaderState *state, Object &object)
{
    object.source_file = SourceFile(state->filepath, state->stream.Max());
    state->stream.Read(object.source_file.GetBuffer(), object.source_file.GetSize());

    return {};
}

std::unique_ptr<Script> ScriptLoader::BuildFn(Engine *engine, const Object &object)
{
    return std::make_unique<Script>(object.source_file);
}

} // namespace hyperion::v2
