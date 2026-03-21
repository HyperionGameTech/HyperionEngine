/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/filesystem/FilePath.hpp>
#include <Core/cli/CommandLine.hpp>
#include <Core/config/Config.hpp>

namespace Hyperion {
namespace CoreApi {

FilePath GetExecutablePath();
void SetExecutablePath(const FilePath& path);

FilePath GetConfigDirectory();
void SetConfigDirectory(const FilePath& configDirectory);

HYP_NODISCARD FilePath CreateTempDirectory();

bool Initialize(int argc, char** argv);

const CommandLineArguments& GetCommandLineArguments();
const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions();

#if HYP_ENABLE_PROFILE
bool IsProfilingEnabled();
#else
static constexpr std::false_type IsProfilingEnabled;
#endif

const GlobalConfig& GetGlobalConfig();
void UpdateGlobalConfig(const ConfigBase& mergeValues);

void OnShutdown(void (*func)());

void Shutdown();

} // namespace CoreApi
} // namespace Hyperion