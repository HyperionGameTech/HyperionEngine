#ifndef BYTECODE_UTIL_HPP
#define BYTECODE_UTIL_HPP

#include <memory>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

// fwd declarations
struct BytecodeChunk;
struct BuildParams;
struct Buildable;

class BytecodeUtil {
public:
    template<typename T, typename... Args>
    static std::unique_ptr<T> Make(Args&&... args)
    {
        static_assert(std::is_base_of<Buildable, T>::value, "Must be a Buildable type.");
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
};

} // namespace hyperion::compiler

#endif
