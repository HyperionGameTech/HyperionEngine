/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

#include <core/config/Config.hpp>

#include <engine/EngineMemory.hpp>

namespace hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

namespace cli {

class CommandLineArguments;

} // namespace cli

using cli::CommandLineArguments;

namespace sys {

class AppContextBase;
class ApplicationWindow;
struct WindowOptions;

} // namespace sys

using sys::AppContextBase;
using sys::ApplicationWindow;
using sys::WindowOptions;

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

    HYP_API ApplicationWindow* Hyp_CreateWindow(AppContextBase* pCtx, WindowOptions* pWindowOptions, HWND parentHwnd);
    HYP_API void Hyp_DestroyWindow(AppContextBase* pCtx, ApplicationWindow* pWindow);
    HYP_API int Hyp_SetMainWindow(AppContextBase* pCtx, ApplicationWindow* pWindow);
    HYP_API ApplicationWindow* Hyp_GetMainWindow(AppContextBase* pCtx);
    HYP_API HWND Hyp_GetHWND(ApplicationWindow* pWindow);

#ifdef HYP_MACOS
    /// @brief Get the NSView associated with an embedded Cocoa window
    /// @param pWindow The application window
    /// @return The NSView pointer, or nullptr if not an embedded view
    HYP_API void* Hyp_GetNSView(ApplicationWindow* pWindow);
#endif

    HYP_API Game* Hyp_CreateGame(const char* gameClassName);
    HYP_API void Hyp_DestroyGame(Game* pGame);
    HYP_API int Hyp_LaunchGame(Game* pGame);
}

} // namespace hyperion
