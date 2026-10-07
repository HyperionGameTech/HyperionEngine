/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_C_BINDING_GENERATOR_HPP
#define HYPERION_CODEGEN_C_BINDING_GENERATOR_HPP

#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class ByteWriter;

namespace CodeGen {

class Analyzer;
class Module;

// Generates the engine's C bindings: one `extern "C"` thunk per bindable reflected method or
// property accessor, registered by name so other languages can resolve it.
class CBindingGenerator
{
public:
    // Appends this module's thunks to its .generated.inl.
    Result EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& declaredTypeNames, ByteWriter& writer) const;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
