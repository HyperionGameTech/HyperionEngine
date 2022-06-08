#ifndef HYPERION_V2_HYPSCRIPT_LOADER_H
#define HYPERION_V2_HYPSCRIPT_LOADER_H

#include <asset/loader_object.h>
#include <asset/loader.h>

#include <script/Script.hpp>

namespace hyperion::v2 {

using namespace compiler;

template <>
struct LoaderObject<Script, LoaderFormat::SCRIPT_HYPSCRIPT> {
    class Loader : public LoaderBase<Script, LoaderFormat::SCRIPT_HYPSCRIPT> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Script> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    SourceFile source_file;
};

} // namespace hyperion::v2

#endif
