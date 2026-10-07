/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_C_BINDING_GENERATOR_HPP
#define HYPERION_CODEGEN_C_BINDING_GENERATOR_HPP

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class ByteWriter;

namespace CodeGen {

class Analyzer;
class Module;
class BindingLayoutSet;

struct ThunkBindingValue
{
    String name;
    String type;
    String convention;
};

struct ThunkBindingRecord
{
    String symbol;
    String className;
    String memberName;
    String kind;
    String condition;
    bool isStatic = false;
    Array<ThunkBindingValue> params;
    ThunkBindingValue returnValue;

    // For a "result" return that carries a value (TResult<T>): how T comes back through `outValue`.
    bool hasResultValue = false;
    ThunkBindingValue resultValue;
};

// Generates the engine's C bindings: one `extern "C"` thunk per bindable reflected method or property accessor,
// registered by name so any language can resolve it (Hyp_ResolveBinding), plus a manifest describing them.
class CBindingGenerator
{
public:
    // Appends this module's thunks to its .generated.inl.
    Result EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& declaredTypeNames, ByteWriter& writer, Array<ThunkBindingRecord>* outRecords = nullptr) const;

    // Appends compile-time checks that this module's structs still have the layout C and Rust were given.
    Result EmitLayoutChecks(const Module& mod, const BindingLayoutSet& layouts, ByteWriter& writer) const;

    // Every binding collected by EmitThunks as JSON, for generators of other languages to consume.
    static String FormatBindingManifest(Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts);
};

} // namespace CodeGen
} // namespace Hyperion

#endif
