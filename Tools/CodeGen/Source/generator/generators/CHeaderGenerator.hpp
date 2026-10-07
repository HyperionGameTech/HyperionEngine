/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_C_HEADER_GENERATOR_HPP
#define HYPERION_CODEGEN_C_HEADER_GENERATOR_HPP

#include <generator/generators/CBindingGenerator.hpp>

namespace Hyperion {
namespace CodeGen {

// Writes the C bindings as a plain C header: opaque object types, enum typedefs and a struct of function pointers
// with a loader that resolves each by name.
class CHeaderGenerator
{
public:
    static String Format(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts);
};

} // namespace CodeGen
} // namespace Hyperion

#endif
