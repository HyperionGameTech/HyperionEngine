/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_CLANG_DRIVER_HPP
#define HYPERION_BUILDTOOL_CLANG_DRIVER_HPP

#include <driver/Driver.hpp>

namespace hyperion {
namespace buildtool {

class ClangDriver : public IDriver
{
public:
    virtual ~ClangDriver() override = default;

    virtual Result<void, AnalyzerError> ProcessModule(const Analyzer &analyzer, Module &mod) override;
};

} // namespace buildtool
} // namespace hyperion

#endif
