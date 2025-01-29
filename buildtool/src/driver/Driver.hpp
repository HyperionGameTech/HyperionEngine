/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_DRIVER_HPP
#define HYPERION_BUILDTOOL_DRIVER_HPP

#include <analyzer/AnalyzerError.hpp>

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/filesystem/FilePath.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

namespace buildtool {

class Analyzer;
class Module;

class IDriver
{
public:
    virtual ~IDriver() = default;

    virtual Result<void, AnalyzerError> ProcessModule(const Analyzer &analyzer, Module &mod) = 0;
};

} // namespace buildtool

} // namespace hyperion

#endif
