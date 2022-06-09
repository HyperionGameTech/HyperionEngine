#ifndef META_HPP
#define META_HPP

namespace hyperion {
namespace vm {
    class VM;
}

class APIInstance;
namespace compiler {
class CompilationUnit;

class Meta {
public:
    static void BuildMetaLibrary(hyperion::vm::VM &vm,
        CompilationUnit &compilation_unit,
        hyperion::APIInstance &api);
};
} // namespace compiler
} // namespace hyperion

#endif
