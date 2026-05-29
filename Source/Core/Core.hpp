/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/filesystem/FilePath.hpp>
#include <Core/cli/CommandLine.hpp>
#include <Core/config/Config.hpp>

namespace Hyperion {
namespace CoreApi {

CORE_API FilePath GetExecutablePath();
CORE_API void SetExecutablePath(const FilePath& path);

CORE_API FilePath GetConfigDirectory();
CORE_API void SetConfigDirectory(const FilePath& configDirectory);

HYP_NODISCARD CORE_API FilePath CreateTempDirectory();

CORE_API bool Initialize(int argc, char** argv);

CORE_API const CommandLineArguments& GetCommandLineArguments();
CORE_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions();

#if HYP_ENABLE_PROFILE
CORE_API bool IsProfilingEnabled();
#else
static constexpr std::false_type IsProfilingEnabled;
#endif

CORE_API const GlobalConfig& GetGlobalConfig();
CORE_API void UpdateGlobalConfig(const ConfigBase& mergeValues);

CORE_API void OnShutdown(void (*func)());

CORE_API void Shutdown();

} // namespace CoreApi
} // namespace Hyperion