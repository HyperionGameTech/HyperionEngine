/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Defines.hpp>

#include <Core/Config/Config.hpp>

#include <Framework/EngineMemory.hpp>

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

#if HYP_DOTNET
struct ManagedDelegates;
using InitFromManagedCallback = void (*)(struct ManagedDelegates*);
#endif

ENGINE_API const FilePath& GetLibraryDirectory();
ENGINE_API const FilePath& GetProjectsDirectory();
ENGINE_API const FilePath& GetDataDirectory();
ENGINE_API const FilePath& GetCacheDirectory();
ENGINE_API const FilePath& GetTempDirectory();

#if !HYP_WINDOWS
using HWND = void*;
#endif

extern "C"
{
    ENGINE_API int Hyp_Initialize(int argc, char** argv);
    ENGINE_API void Hyp_Shutdown();

    ENGINE_API AppContextBase* Hyp_GetAppContext();

    ENGINE_API Game* Hyp_CreateGame(const char* gameClassName);
    ENGINE_API void Hyp_DestroyGame(Game* pGame);
    ENGINE_API void Hyp_SetGame(Game* pGame);

    ENGINE_API int Hyp_LaunchThreads();
    ENGINE_API void Hyp_StopThreads();

    // Only for use in detached mode (-Detached CLI flag)
    ENGINE_API void Hyp_MainThreadUpdate();

#if HYP_DOTNET
    ENGINE_API void Hyp_SetInitFromManagedCallback(InitFromManagedCallback callback);
#endif

#if HYP_ANDROID
    ENGINE_API void Hyp_SetAssetManager(void* mgr);
    ENGINE_API void Hyp_SetNativeWindow(void* nativeWindow);
    ENGINE_API void Hyp_InputEvent(int type, int action, float x, float y, int iParam);
#endif
}

} // namespace Hyperion
