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
struct HypGameCallbacks;

#if HYP_DOTNET
struct ManagedDelegates;
using InitFromManagedCallback = void (*)(struct ManagedDelegates*);
#endif

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

    // Only for use in detached mode (--detached CLI flag)
    ENGINE_API void Hyp_MainThreadUpdate();

    // Detached mode: set when the main window closes or on ctrl-c. The host should stop pumping and call Hyp_Shutdown().
    ENGINE_API int Hyp_IsQuitRequested();
    ENGINE_API void Hyp_RequestQuit();

    // Generated C bindings (see Source/Generated/Bindings.json): returns the function for a symbol such as "Node_GetName", or null.
    ENGINE_API void* Hyp_ResolveBinding(const char* name);
    ENGINE_API int Hyp_GetBindingAbiVersion();

    // Reference counting for objects that cross the C bindings. A binding whose manifest return convention is
    // "handle_retained" returns an object the caller must Hyp_Release.
    ENGINE_API void Hyp_Retain(void* object);
    ENGINE_API void Hyp_Release(void* object);

    // Frees a string or array that a binding returned through an out-parameter.
    ENGINE_API void Hyp_Free(void* data);

    // A Game driven by C function pointers. Returns a retained object, released with Hyp_DestroyGame.
    ENGINE_API Game* Hyp_CreateCallbackGame(const HypGameCallbacks* callbacks, void* userData);

    ENGINE_API void Hyp_Log(int level, const char* message);

    ENGINE_API void Hyp_GetEngineVersion(unsigned int* outMajor, unsigned int* outMinor, unsigned int* outPatch);

#ifdef HYP_DOTNET
    ENGINE_API void Hyp_SetInitFromManagedCallback(InitFromManagedCallback callback);
#endif

#ifdef HYP_ANDROID
    ENGINE_API void Hyp_SetAssetManager(void* mgr);
    ENGINE_API void Hyp_SetNativeWindow(void* nativeWindow, int width, int height);
    ENGINE_API void Hyp_InputEvent(int type, int action, float x, float y, int iParam);
#endif
}

} // namespace Hyperion
