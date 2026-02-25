/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#ifndef HYPERION_CODEGEN_CSHARP_MODULE_GENERATOR_HPP
#define HYPERION_CODEGEN_CSHARP_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

namespace Hyperion {
namespace CodeGen {

class CSharpModuleGenerator : public GeneratorBase
{
public:
    virtual ~CSharpModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

protected:
};

} // namespace CodeGen
} // namespace Hyperion

#endif
