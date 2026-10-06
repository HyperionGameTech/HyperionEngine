/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

using GetGameClassNameFn = const char* (*)();
using GetEngineVersionFn = void (*)(uint32*, uint32*, uint32*);

struct LoadedNativeModule
{
    FilePath modulePath;
    String gameClassName;
    String error;
    Time loadedTimestamp;
};

// Modules are never unloaded: their classes stay registered.
static Array<LoadedNativeModule> s_loadedModules;
static Mutex s_loadedModulesMutex;
static uint32 s_moduleCopyCounter = 0;

} // namespace

EDITOR_API String GetNativeProjectName(const FilePath& projectFilepath)
{
    const String baseName = FilePath(projectFilepath.Basename()).StripExtension();

    String name;

    for (const char* it = baseName.Data(); *it != 0; ++it)
    {
        const char ch = *it;

        const bool isAlpha = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        const bool isDigit = ch >= '0' && ch <= '9';

        if (isAlpha || ch == '_' || (isDigit && name.Any()))
        {
            name.Append(ch);
        }
    }

    if (name.Empty())
    {
        name = "MyGame";
    }

    return name;
}

EDITOR_API FilePath GetNativeModulePath(const FilePath& projectFilepath)
{
#ifdef HYP_WINDOWS
    const String fileName = GetNativeProjectName(projectFilepath) + "Game.dll";
#elif defined(HYP_MACOS)
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.dylib";
#else
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.so";
#endif

    // matches GAME_OUTPUT_DIR in the generated CMakeLists.txt (CMAKE_SYSTEM_NAME)
#ifdef HYP_WINDOWS
    const char* platformDirectory = "Windows";
#elif defined(HYP_MACOS)
    const char* platformDirectory = "Darwin";
#else
    const char* platformDirectory = "Linux";
#endif

    return projectFilepath.BasePath() / "Binaries" / platformDirectory / fileName;
}

EDITOR_API TResult<Handle<Game>> CreateGameFromNativeModule(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetNativeModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    LoadedNativeModule* loadedModulePtr = nullptr;

    for (LoadedNativeModule& candidate : s_loadedModules)
    {
        if (candidate.modulePath == modulePath)
        {
            loadedModulePtr = &candidate;

            break;
        }
    }

    if (loadedModulePtr == nullptr)
    {
        // Load a copy so the project can be rebuilt while the editor has the module loaded
        const FilePath copyDirectory = EngineGlobals::GetTempDirectory() / "NativeModules";

        if (!copyDirectory.Exists() && !copyDirectory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create {} for the game module", copyDirectory);
        }

        const FilePath copyPath = copyDirectory / HYP_FORMAT("{}.{}.{}{}",
            FilePath(modulePath.Basename()).StripExtension(),
            uint64(Time::Now()),
            s_moduleCopyCounter++,
            modulePath.GetExtension().Any() ? String(".") + modulePath.GetExtension() : String());

        std::error_code errorCode;
        std::filesystem::copy_file(modulePath.Data(), copyPath.Data(), std::filesystem::copy_options::overwrite_existing, errorCode);

        if (errorCode)
        {
            return HYP_MAKE_ERROR(Error, "Failed to copy game module {} to {}: {}", modulePath, copyPath, errorCode.message().c_str());
        }

        DynamicLibrary* library = DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(copyPath)));

        if (library == nullptr)
        {
            return HYP_MAKE_ERROR(Error, "Failed to load game module {}", modulePath);
        }

        // from here on the module is loaded for good, so a failure is remembered rather than retried
        LoadedNativeModule loadedModule;
        loadedModule.modulePath = modulePath;
        loadedModule.loadedTimestamp = modulePath.LastModifiedTimestamp();

        const GetGameClassNameFn getGameClassName = reinterpret_cast<GetGameClassNameFn>(library->GetFunction("HypGameModule_GetGameClassName"));
        const GetEngineVersionFn getEngineVersion = reinterpret_cast<GetEngineVersionFn>(library->GetFunction("HypGameModule_GetEngineVersion"));

        uint32 versionMajor = 0;
        uint32 versionMinor = 0;
        uint32 versionPatch = 0;

        if (getGameClassName == nullptr || getEngineVersion == nullptr)
        {
            loadedModule.error = HYP_FORMAT("Game module {} doesn't export HypGameModule_GetGameClassName / HypGameModule_GetEngineVersion", modulePath);
        }
        else if (getEngineVersion(&versionMajor, &versionMinor, &versionPatch);
            versionMajor != HYP_VERSION_MAJOR || versionMinor != HYP_VERSION_MINOR)
        {
            loadedModule.error = HYP_FORMAT("Game module {} was built for engine {}.{}.{} (this is {}.{}.{}). Rebuild the game and restart the editor.",
                modulePath, versionMajor, versionMinor, versionPatch, HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
        }
        else
        {
            loadedModule.gameClassName = getGameClassName();

            const Class* gameClass = GetClass(StringHash(loadedModule.gameClassName.Data()));

            if (gameClass == nullptr || !gameClass->IsDerivedFrom(Game::StaticClass()))
            {
                loadedModule.error = HYP_FORMAT("Game module {} names Game class '{}', which isn't a registered Game subclass", modulePath, loadedModule.gameClassName);
            }
            else
            {
#ifdef HYP_DOTNET
                // The class has no C# counterpart, so the managed editor sees instances of it as plain Game objects
                if (gameClass->GetManagedClass() == nullptr)
                {
                    gameClass->SetManagedClass(Game::StaticClass()->GetManagedClass());
                }
#endif

                HYP_LOG(Editor, Info, "Loaded game module {} (Game class: {})", modulePath, loadedModule.gameClassName);
            }
        }

        loadedModulePtr = &s_loadedModules.PushBack(std::move(loadedModule));
    }
    else if (modulePath.LastModifiedTimestamp() > loadedModulePtr->loadedTimestamp)
    {
        HYP_LOG(Editor, Warning, "Game module {} was rebuilt since it was loaded, restart the editor to use the new build", modulePath);
    }

    if (loadedModulePtr->error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", loadedModulePtr->error);
    }

    Handle<Game> game = Game::CreateGame(StringHash(loadedModulePtr->gameClassName.Data()));

    if (!game.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create an instance of Game class '{}' from {}", loadedModulePtr->gameClassName, modulePath);
    }

    return game;
}

} // namespace Hyperion
