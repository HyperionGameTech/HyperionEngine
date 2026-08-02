/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_STRATA_MODULE_GENERATOR_HPP
#define HYPERION_CODEGEN_STRATA_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {
namespace CodeGen {

struct ClassDefinition;

class StrataModuleGenerator : public GeneratorBase
{
public:
    virtual ~StrataModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

    // Collect the names of every scriptable class/struct across all modules
    Set<String> CollectHandleNames(const Analyzer& analyzer) const;

    // Collect every unreflected C++ type name encountered in bindable method
    // signatures. These become `struct Name;` forward declarations.
    Set<String> CollectForwardStructNames(const Analyzer& analyzer) const;

    // Resolve the base handle a class should `extends`, or an empty string if it
    // has none.
    String ResolveHandleBase(const Analyzer& analyzer, const ClassDefinition& cls, const Set<String>& allHandleNames) const;

    // Emits `handle <Name>;` (or `handle <Name> extends <Base>;`) for each
    // scriptable class/struct in this module.
    Result EmitHandles(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const;

    // Emit `struct <Name>;` forward declarations for unreflected struct types.
    Result EmitForwardStructDeclarations(const Analyzer& analyzer, const Set<String>& allStructNames, ByteWriter& writer) const;

    // Emit one `extern` free function per bindable method in this module.
    Result EmitMethods(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer) const;

    //  Generates C bindings for our C++ methods.
    Result EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer) const;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
