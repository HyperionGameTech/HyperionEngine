/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

#include <core/config/Config.hpp>

#include <engine/EngineMemory.hpp>

namespace Hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

namespace cli {

class CommandLineArguments;

} // namespace cli

using cli::CommandLineArguments;

class AppContextBase;
class ApplicationWindow;
struct WindowOptions;

class Game;

HYP_API const FilePath& GetResourceDirectory();
HYP_API const FilePath& GetCacheDirectory();
HYP_API const FilePath& GetTempDirectory();

#ifndef HYP_WINDOWS
using HWND = void*;
#endif

extern "C"
{
    HYP_API int Hyp_Initialize(int argc, char** argv);
    HYP_API void Hyp_Shutdown();

    HYP_API AppContextBase* Hyp_GetAppContext();

    HYP_API Game* Hyp_CreateGame(const char* gameClassName);
    HYP_API void Hyp_DestroyGame(Game* pGame);
    HYP_API void Hyp_SetGame(Game* pGame);
    HYP_API int Hyp_LaunchThreads();

    // Only for use in detached mode (-Detached CLI flag)
    HYP_API void Hyp_MainThreadUpdate();
}

} // namespace Hyperion
