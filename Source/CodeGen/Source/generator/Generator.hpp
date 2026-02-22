/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CODEGEN_GENERATOR_HPP
#define HYPERION_CODEGEN_GENERATOR_HPP

#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/Defines.hpp>

namespace Hyperion {

class ByteWriter;

namespace CodeGen {

class Analyzer;
class Module;

class GeneratorBase
{
public:
    virtual ~GeneratorBase() = default;

    Result Generate(const Analyzer& analyzer, const Module& mod) const;

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const = 0;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const = 0;

protected:
};

} // namespace CodeGen
} // namespace Hyperion

#endif
