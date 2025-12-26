/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/filesystem/FilePath.hpp>
#include <core/cli/CommandLine.hpp>
#include <core/config/Config.hpp>

namespace Hyperion {

FilePath CoreApi_GetExecutablePath();
void CoreApi_SetExecutablePath(const FilePath& path);

bool CoreApi_Initialize(int argc, char** argv);

const CommandLineArguments& CoreApi_GetCommandLineArguments();
const CommandLineArgumentDefinitions& CoreApi_DefaultCommandLineArgumentDefinitions();

const GlobalConfig& CoreApi_GetGlobalConfig();
void CoreApi_UpdateGlobalConfig(const ConfigurationTable& mergeValues);

void CoreApi_OnShutdown(void (*func)());

void CoreApi_Shutdown();

} // namespace Hyperion