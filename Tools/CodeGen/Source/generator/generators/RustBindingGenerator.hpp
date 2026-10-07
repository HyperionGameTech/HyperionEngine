/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_RUST_BINDING_GENERATOR_HPP
#define HYPERION_CODEGEN_RUST_BINDING_GENERATOR_HPP

#include <generator/generators/CBindingGenerator.hpp>

namespace Hyperion {
namespace CodeGen {

// Writes the raw Rust view of the C bindings (included by the hyperion-sys crate): opaque object types, enum
// aliases and a table of function pointers resolved by name at startup.
class RustBindingGenerator
{
public:
    static String Format(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts);

    // A safe wrapper type per engine class over the raw table (included by the hyperion crate).
    static String FormatWrappers(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts);
};

} // namespace CodeGen
} // namespace Hyperion

#endif
