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
#include <Core/Reflection/ScriptObjectFunctions.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedClass.hpp>
#endif

#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

namespace {

using GetGameClassNameFn = const char* (*)();
using CreateGameFn = Game* (*)();
using GetEngineVersionFn = void (*)(uint32*, uint32*, uint32*);

struct LoadedNativeModule
{
    FilePath modulePath;
    String gameClassName;
    CreateGameFn createGame = nullptr;
    String error;
    Time loadedTimestamp;
};

// Modules are never unloaded: their classes stay registered.
static Array<LoadedNativeModule> s_loadedModules;
static Mutex s_loadedModulesMutex;
static uint32 s_moduleCopyCounter = 0;

static const char* GetModulePlatformDirectory()
{
#ifdef HYP_WINDOWS
    return "Windows";
#elif defined(HYP_MACOS)
    return "Darwin";
#else
    return "Linux";
#endif
}

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

    return projectFilepath.BasePath() / "Binaries" / GetModulePlatformDirectory() / fileName;
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

        // a module without a reflected Game class (e.g. one written in Rust) creates its game itself
        const CreateGameFn createGame = reinterpret_cast<CreateGameFn>(library->GetFunction("HypGameModule_CreateGame"));

        if ((getGameClassName == nullptr && createGame == nullptr) || getEngineVersion == nullptr)
        {
            loadedModule.error = HYP_FORMAT("Game module {} doesn't export HypGameModule_GetGameClassName / HypGameModule_GetEngineVersion", modulePath);
        }
        else if (getEngineVersion(&versionMajor, &versionMinor, &versionPatch);
            versionMajor != HYP_VERSION_MAJOR || versionMinor != HYP_VERSION_MINOR)
        {
            loadedModule.error = HYP_FORMAT("Game module {} was built for engine {}.{}.{} (this is {}.{}.{}). Rebuild the game and restart the editor.",
                modulePath, versionMajor, versionMinor, versionPatch, HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
        }
        else if (createGame != nullptr)
        {
            loadedModule.createGame = createGame;

            HYP_LOG(Editor, Info, "Loaded game module {} (creates its own Game)", modulePath);
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

    Handle<Game> game;

    if (loadedModulePtr->createGame != nullptr)
    {
        // returned with a reference that the handle takes over
        game.ptr = loadedModulePtr->createGame();
    }
    else
    {
        game = Game::CreateGame(StringHash(loadedModulePtr->gameClassName.Data()));
    }

    if (!game.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create an instance of Game class '{}' from {}", loadedModulePtr->gameClassName, modulePath);
    }

    return game;
}

namespace {

#ifdef HYP_DOTNET
struct LoadedManagedModule
{
    FilePath modulePath;
    SharedPtr<dotnet::Assembly> assembly;
    SharedPtr<dotnet::ManagedClass> gameClass;
    String error;
    Time loadedTimestamp;
};

static Array<LoadedManagedModule> s_loadedManagedModules;
#endif

} // namespace

EDITOR_API FilePath GetManagedProjectFilePath(const FilePath& projectFilepath)
{
    return projectFilepath.BasePath() / "Source" / (GetNativeProjectName(projectFilepath) + ".csproj");
}

EDITOR_API bool IsManagedProject(const FilePath& projectFilepath)
{
    return GetManagedProjectFilePath(projectFilepath).Exists();
}

EDITOR_API FilePath GetManagedModulePath(const FilePath& projectFilepath)
{
    return projectFilepath.BasePath() / "Binaries" / GetModulePlatformDirectory() / (GetNativeProjectName(projectFilepath) + ".dll");
}

EDITOR_API Result WriteManagedProjectProps(const FilePath& projectFilepath)
{
    const FilePath propsPath = projectFilepath.BasePath() / "Source" / "Hyperion.Local.props";

    const String binDir = String(CoreApi::GetExecutablePath()).ReplaceAll("\\", "/");
    const String baseDir = String(CoreApi::GetBaseDirectory()).ReplaceAll("\\", "/");

    const String text = String("<Project>\n  <PropertyGroup>\n")
        + "    <HyperionBinDir>" + binDir + "/</HyperionBinDir>\n"
        + "    <HyperionBaseDir>" + baseDir + "/</HyperionBaseDir>\n"
        + "    <HyperionSdkDir>" + baseDir + "/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
        + "  </PropertyGroup>\n</Project>\n";

    FileByteWriter writer { propsPath };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", propsPath);
    }

    writer.Write(text.Data(), text.Size());
    writer.Close();

    return {};
}

EDITOR_API TResult<Handle<Game>> CreateGameFromManagedModule(const FilePath& projectFilepath)
{
#ifdef HYP_DOTNET
    if (!IsManagedProject(projectFilepath))
    {
        return Handle<Game>();
    }

    const FilePath modulePath = GetManagedModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    LoadedManagedModule* loadedModulePtr = nullptr;

    for (LoadedManagedModule& candidate : s_loadedManagedModules)
    {
        if (candidate.modulePath == modulePath)
        {
            loadedModulePtr = &candidate;

            break;
        }
    }

    if (loadedModulePtr == nullptr)
    {
        const FilePath copyDirectory = EngineGlobals::GetTempDirectory() / "ManagedModules";

        if (!copyDirectory.Exists() && !copyDirectory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create {} for the game assembly", copyDirectory);
        }

        const FilePath copyPath = copyDirectory / HYP_FORMAT("{}.{}.{}.dll",
            FilePath(modulePath.Basename()).StripExtension(),
            uint64(Time::Now()),
            s_moduleCopyCounter++);

        std::error_code errorCode;
        std::filesystem::copy_file(modulePath.Data(), copyPath.Data(), std::filesystem::copy_options::overwrite_existing, errorCode);

        if (errorCode)
        {
            return HYP_MAKE_ERROR(Error, "Failed to copy game assembly {} to {}: {}", modulePath, copyPath, errorCode.message().c_str());
        }

        const String gameClassName = GetNativeProjectName(projectFilepath) + "Game";

        LoadedManagedModule loadedModule;
        loadedModule.modulePath = modulePath;
        loadedModule.loadedTimestamp = modulePath.LastModifiedTimestamp();
        loadedModule.assembly = DotNETHost::GetInstance().LoadAssembly(copyPath.Data());

        if (!loadedModule.assembly)
        {
            loadedModule.error = HYP_FORMAT("Failed to load game assembly {} (it may have been built for a different engine version). Rebuild the game and restart the editor.", modulePath);
        }
        else
        {
            loadedModule.gameClass = loadedModule.assembly->FindClassByName(gameClassName.Data());

            if (!loadedModule.gameClass || !loadedModule.gameClass->HasParentClass("Game"))
            {
                loadedModule.gameClass.Reset();
                loadedModule.error = HYP_FORMAT("Game assembly {} has no Game subclass named '{}'", modulePath, gameClassName);
            }
            else
            {
                HYP_LOG(Editor, Info, "Loaded game assembly {} (Game class: {})", modulePath, gameClassName);
            }
        }

        loadedModulePtr = &s_loadedManagedModules.PushBack(std::move(loadedModule));
    }
    else if (modulePath.LastModifiedTimestamp() > loadedModulePtr->loadedTimestamp)
    {
        HYP_LOG(Editor, Warning, "Game assembly {} was rebuilt since it was loaded, restart the editor to use the new build", modulePath);
    }

    if (loadedModulePtr->error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", loadedModulePtr->error);
    }

    Handle<Game> game;

    {
        GlobalContextScope scope(ObjectInitializerContext { Game::StaticClass(), ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        game = MakeHandle<Game>();
    }

    ScriptObjectResource* scriptObjectResource = ScriptObjectFunctions::CreateScriptObjectResource_DotNet(game.Get(), loadedModulePtr->gameClass);

    if (scriptObjectResource == nullptr || ScriptObjectFunctions::GetManagedObject(scriptObjectResource) == nullptr)
    {
        if (scriptObjectResource != nullptr)
        {
            ScriptObjectFunctions::DestroyScriptObjectResource(scriptObjectResource);
        }

        return HYP_MAKE_ERROR(Error, "Failed to create an instance of the Game class from {}", modulePath);
    }

    game->SetScriptObjectResource(scriptObjectResource);
    scriptObjectResource->AddReader();

    return game;
#else
    return Handle<Game>();
#endif
}

namespace {

struct GameProjectFile
{
    FilePath templatePath;
    FilePath filepath;
};

Array<GameProjectFile> GetGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language)
{
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String className = GetNativeProjectName(projectFilepath) + "Game";

    Array<GameProjectFile> files;

    if (language == GameProjectLanguage::Managed)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/ManagedGame";

        files.PushBack({ templateDir / "Game.csproj.in", GetManagedProjectFilePath(projectFilepath) });
        files.PushBack({ templateDir / "Game.cs.in", sourceDir / (className + ".cs") });
        files.PushBack({ templateDir / "Program.cs.in", sourceDir / "Program.cs" });
        files.PushBack({ templateDir / "Game.slnx.in", projectFilepath.BasePath() / (GetNativeProjectName(projectFilepath) + ".slnx") });
    }
    else if (language == GameProjectLanguage::Native)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/NativeGame";

        files.PushBack({ templateDir / "CMakeLists.txt.in", sourceDir / "CMakeLists.txt" });
        files.PushBack({ templateDir / "Game" / "Game.hpp.in", sourceDir / "Game" / (className + ".hpp") });
        files.PushBack({ templateDir / "Game" / "Game.cpp.in", sourceDir / "Game" / (className + ".cpp") });
        files.PushBack({ templateDir / "Launcher" / "main.cpp.in", sourceDir / "Launcher" / "main.cpp" });
    }
    else if (language == GameProjectLanguage::Rust)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/RustGame";

        files.PushBack({ templateDir / "Cargo.toml.in", sourceDir / "Cargo.toml" });
        files.PushBack({ templateDir / "src" / "lib.rs.in", sourceDir / "src" / "lib.rs" });
        files.PushBack({ templateDir / "src" / "main.rs.in", sourceDir / "src" / "main.rs" });
    }

    return files;
}

bool ReadTextFile(const FilePath& filepath, String& outText)
{
    if (!filepath.Exists())
    {
        return false;
    }

    FileByteReader reader { filepath };

    if (reader.Eof())
    {
        return false;
    }

    const ByteBuffer buffer = reader.Read();
    reader.Close();

    outText = String(buffer.ToByteView());

    return true;
}

bool RenderGameProjectTemplate(const FilePath& templatePath, const FilePath& projectFilepath, String& outText)
{
    String templateText;

    if (!ReadTextFile(templatePath, templateText))
    {
        return false;
    }

    const String name = GetNativeProjectName(projectFilepath);

    // the engine's Rust crates (Source/Rust), which a Rust game depends on by path
    const String rustDir = String(CoreApi::GetBaseDirectory() / "Source" / "Rust").ReplaceAll("\\", "/");

    outText = templateText.ReplaceAll("@NAME@", name).ReplaceAll("@CLASS@", name + "Game").ReplaceAll("@HYPERION_RUST_DIR@", rustDir);

    return true;
}

bool IsGameProjectScratchDirectory(const std::filesystem::path& path)
{
    static const char* const s_scratchDirectories[] = { "obj", "bin", ".vs", ".vscode", ".idea" };

    const std::string directoryName = path.filename().string();

    for (const char* scratchDirectory : s_scratchDirectories)
    {
        if (directoryName == scratchDirectory)
        {
            return true;
        }
    }

    return false;
}

} // namespace

EDITOR_API GameProjectLanguage GetGameProjectLanguage(const FilePath& projectFilepath)
{
    if (!(projectFilepath.BasePath() / "Source").Exists())
    {
        return GameProjectLanguage::None;
    }

    if ((projectFilepath.BasePath() / "Source" / "Cargo.toml").Exists())
    {
        return GameProjectLanguage::Rust;
    }

    return IsManagedProject(projectFilepath) ? GameProjectLanguage::Managed : GameProjectLanguage::Native;
}

EDITOR_API const char* GetGameProjectLanguageName(GameProjectLanguage language)
{
    switch (language)
    {
    case GameProjectLanguage::Managed:
        return "C#";
    case GameProjectLanguage::Rust:
        return "Rust";
    default:
        return "C++";
    }
}

EDITOR_API bool IsGameProjectUnmodified(const FilePath& projectFilepath)
{
    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);

    if (language == GameProjectLanguage::None)
    {
        return false;
    }

    const FilePath sourceDir = projectFilepath.BasePath() / "Source";

    std::error_code errorCode;
    Array<std::filesystem::path> generatedPaths;

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language))
    {
        String expectedText;
        String actualText;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, expectedText)
            || !ReadTextFile(file.filepath, actualText)
            || expectedText != actualText)
        {
            return false;
        }

        generatedPaths.PushBack(std::filesystem::weakly_canonical(file.filepath.Data(), errorCode));
    }

    generatedPaths.PushBack(std::filesystem::weakly_canonical((sourceDir / "Hyperion.Local.props").Data(), errorCode));
    generatedPaths.PushBack(std::filesystem::weakly_canonical((sourceDir / "CMakeUserPresets.json").Data(), errorCode));
    generatedPaths.PushBack(std::filesystem::weakly_canonical((sourceDir / "Cargo.lock").Data(), errorCode));

    // anything the user added next to the generated files counts as their work too
    for (std::filesystem::recursive_directory_iterator it(sourceDir.Data(), errorCode), end; !errorCode && it != end; it.increment(errorCode))
    {
        if (it->is_directory(errorCode))
        {
            if (IsGameProjectScratchDirectory(it->path()))
            {
                it.disable_recursion_pending();
            }

            continue;
        }

        const std::filesystem::path path = std::filesystem::weakly_canonical(it->path(), errorCode);

        bool isGenerated = false;

        for (const std::filesystem::path& generatedPath : generatedPaths)
        {
            isGenerated |= (path == generatedPath);
        }

        if (!isGenerated)
        {
            return false;
        }
    }

    return !errorCode;
}

EDITOR_API Result GenerateGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language)
{
    if (language == GameProjectLanguage::None)
    {
        return HYP_MAKE_ERROR(Error, "No game project language given");
    }

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language))
    {
        String text;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, text))
        {
            return HYP_MAKE_ERROR(Error, "Template {} is missing", file.templatePath);
        }

        const FilePath directory = file.filepath.BasePath();

        if (!directory.Exists() && !directory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create directory {}", directory);
        }

        FileByteWriter writer { file.filepath };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", file.filepath);
        }

        writer.Write(text.Data(), text.Size());
        writer.Close();
    }

    if (language == GameProjectLanguage::Managed)
    {
        return WriteManagedProjectProps(projectFilepath);
    }

    return {};
}

EDITOR_API Result RemoveGameProjectFiles(const FilePath& projectFilepath)
{
    if (!IsGameProjectUnmodified(projectFilepath))
    {
        return HYP_MAKE_ERROR(Error, "The game project has been modified, not removing it");
    }

    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";

    if (!sourceDir.RemoveRecursively())
    {
        return HYP_MAKE_ERROR(Error, "Failed to remove {}", sourceDir);
    }

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language))
    {
        if (file.filepath.Exists())
        {
            file.filepath.Remove();
        }
    }

    // a module built from the removed sources would still be picked up for play
    const FilePath modulePath = language == GameProjectLanguage::Managed
        ? GetManagedModulePath(projectFilepath)
        : GetNativeModulePath(projectFilepath);

    if (modulePath.Exists())
    {
        modulePath.Remove();
    }

    return {};
}

} // namespace Hyperion
