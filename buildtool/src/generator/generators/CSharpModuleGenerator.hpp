/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_CSHARP_MODULE_GENERATOR_HPP
#define HYPERION_BUILDTOOL_CSHARP_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

namespace hyperion {
namespace buildtool {

class CSharpModuleGenerator : public GeneratorBase
{
public:
    virtual ~CSharpModuleGenerator() override = default;

protected:
    virtual FilePath GetOutputFilePath(const Analyzer &analyzer, const Module &mod) const override;

    virtual Result<void> Generate_Internal(const Analyzer &analyzer, const Module &mod, ByteWriter &writer) const override;
};

} // namespace buildtool
} // namespace hyperion

#endif
