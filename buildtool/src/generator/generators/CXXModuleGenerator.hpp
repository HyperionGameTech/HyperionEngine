/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_CXX_MODULE_GENERATOR_HPP
#define HYPERION_BUILDTOOL_CXX_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

namespace hyperion {
namespace buildtool {

class CXXModuleGenerator : public GeneratorBase
{
public:
    virtual ~CXXModuleGenerator() override = default;

protected:
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

    virtual Result Generate_Internal(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
};

} // namespace buildtool
} // namespace hyperion

#endif
