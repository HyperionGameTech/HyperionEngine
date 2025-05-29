/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_GENERATOR_HPP
#define HYPERION_BUILDTOOL_GENERATOR_HPP

#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class ByteWriter;

namespace buildtool {

class Analyzer;
class Module;

class GeneratorBase
{
public:
    virtual ~GeneratorBase() = default;

    Result Generate(const Analyzer& analyzer, const Module& mod) const;

protected:
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const = 0;

    virtual Result Generate_Internal(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const = 0;
};

} // namespace buildtool
} // namespace hyperion

#endif
