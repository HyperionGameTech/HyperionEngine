/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/filesystem/FilePath.hpp>
#include <core/cli/CommandLine.hpp>
#include <core/config/Config.hpp>

namespace Hyperion {
namespace CoreApi {

FilePath GetExecutablePath();
void SetExecutablePath(const FilePath& path);

HYP_NODISCARD FilePath CreateTempDirectory();

bool Initialize(int argc, char** argv);

const CommandLineArguments& GetCommandLineArguments();
const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions();

const GlobalConfig& GetGlobalConfig();
void UpdateGlobalConfig(const ConfigurationTable& mergeValues);

void OnShutdown(void (*func)());

void Shutdown();

} // namespace CoreApi
} // namespace Hyperion