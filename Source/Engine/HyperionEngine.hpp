/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/Defines.hpp>

#include <Core/config/Config.hpp>

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

HYP_API const FilePath& GetLibraryDirectory();
HYP_API const FilePath& GetProjectsDirectory();
HYP_API const FilePath& GetDataDirectory();
HYP_API const FilePath& GetConfigDirectory();
HYP_API const FilePath& GetCacheDirectory();
HYP_API const FilePath& GetTempDirectory();

#if !HYP_WINDOWS
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
    HYP_API void Hyp_StopThreads();

    // Only for use in detached mode (-Detached CLI flag)
    HYP_API void Hyp_MainThreadUpdate();

#if HYP_ANDROID
    HYP_API void Hyp_SetAssetManager(void* mgr);
    HYP_API void Hyp_SetNativeWindow(void* nativeWindow);
    HYP_API void Hyp_InputEvent(int type, int action, float x, float y, int iParam);
#endif
}

} // namespace Hyperion
