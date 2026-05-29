/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_CXX_MODULE_GENERATOR_HPP
#define HYPERION_CODEGEN_CXX_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

namespace Hyperion {
namespace CodeGen {

struct ModuleAPIMapping
{
    String subdirPattern;
    String apiMacro;
    String outputSubdir;
    String initSuffix;
};

class CXXModuleGenerator : public GeneratorBase
{
public:
    virtual ~CXXModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }
    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

    // Inline (.inl) generation helpers for non-jumbo mode
    Result GenerateInline(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const;
    FilePath GetInlineOutputFilePath(const Analyzer& analyzer, const Module& mod) const;

    static const Array<ModuleAPIMapping>& GetModuleAPIMappings();
    static String GetAPIMacroForModule(const Analyzer& analyzer, const Module& mod);
    static String GetClassDeclsOutputSubdirForAPIMacro(const String& apiMacro);
    static String GetInitFunctionNameForAPIMacro(const String& apiMacro);

    Result GenerateClassDeclHeader(const Analyzer& analyzer, ByteWriter& writer) const;
    Result GenerateClassDeclImplementation(const Analyzer& analyzer, const String& apiMacro, ByteWriter& writer) const;

protected:
};

} // namespace CodeGen
} // namespace Hyperion

#endif
