#ifndef JIT_COMPILER_HPP
#define JIT_COMPILER_HPP

namespace hyperion {
namespace jit {

class JitCompiler {
public:
    JitCompiler();
    JitCompiler(const JitCompiler &other) = delete;
    ~JitCompiler();


};

} // namespace jit
} // namespace hyperion

#endif
