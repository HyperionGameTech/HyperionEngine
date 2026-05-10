/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_HYPSCRIPT_MODULE_GENERATOR_HPP
#define HYPERION_CODEGEN_HYPSCRIPT_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Map.hpp>
#include <Core/containers/String.hpp>

namespace Hyperion {
namespace CodeGen {

struct ClassDefinition;

class HypScriptModuleGenerator : public GeneratorBase
{
public:
    virtual ~HypScriptModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

protected:
    Array<const ClassDefinition*> SortClassesTopologically(const Analyzer& analyzer, const Array<const ClassDefinition*>& classes, const TMap<String, size_t>& classNameToIndex) const;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
