/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorGameModuleCommon.hpp>
#include <Editor/EditorProject.hpp>

#include <Framework/Game.hpp>

#include <Scene/World.hpp>
#include <Scene/Systems/ScriptSystem.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Reflection/ScriptObjectFunctions.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedClass.hpp>
#endif

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

#ifdef HYP_DOTNET

enum class ManagedModuleFlags : uint32
{
    None = 0x0,
    NeedsRetarget = 0x1,
    ApplyFailed = 0x2
};

HYP_MAKE_ENUM_FLAGS(ManagedModuleFlags);

namespace {

struct LoadedManagedModule : GameModuleBase
{
    SharedPtr<dotnet::Assembly> assembly;
    SharedPtr<dotnet::ManagedClass> gameClass;
    EnumFlags<ManagedModuleFlags> flags = ManagedModuleFlags::None;
};

Array<UniquePtr<LoadedManagedModule>>& GetLoadedManagedModules()
{
    static Array<UniquePtr<LoadedManagedModule>> s_loadedManagedModules;

    return s_loadedManagedModules;
}

void LoadManagedModuleCopy(
    LoadedManagedModule& loadedModule,
    const FilePath& copyPath,
    const String& gameClassName)
{
    loadedModule.assembly = DotNETHost::GetInstance().LoadAssembly(copyPath.Data());

    if (!loadedModule.assembly)
    {
        loadedModule.loadedTimestampOrError = HYP_MAKE_ERROR(GameModuleError,
            "Failed to load game assembly {} (it may have been built for a different engine version). Rebuild the game.",
            loadedModule.modulePath);

        return;
    }

    loadedModule.gameClass = loadedModule.assembly->FindClassByName(gameClassName.Data());

    if (!loadedModule.gameClass || !loadedModule.gameClass->HasParentClass("Game"))
    {
        loadedModule.gameClass.Reset();
        loadedModule.loadedTimestampOrError = HYP_MAKE_ERROR(GameModuleError,
            "Game assembly {} has no Game subclass named '{}'",
            loadedModule.modulePath, gameClassName);
    }
}

// superseded builds of the assembly aren't unloaded yet, as nothing tracks when their types stop being used
TResult<LoadedManagedModule*> LoadLatestManagedModule(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetManagedModulePath(projectFilepath);
    const Time timestamp = modulePath.LastModifiedTimestamp();

    LoadedManagedModule* currentModule = FindGameModule(GetLoadedManagedModules(), modulePath);

    if (currentModule != nullptr && timestamp <= currentModule->attemptedTimestamp)
    {
        if (currentModule->IsLoaded())
        {
            ScriptSystem::SetGameAssembly(projectFilepath.BasePath(), modulePath, /* isNewBuild */ false);
        }

        return currentModule;
    }

    TResult<FilePath> copyResult = CopyModule(modulePath, "ManagedModules");

    if (copyResult.HasError())
    {
        // most likely still being written by the build
        if (currentModule != nullptr && currentModule->IsLoaded())
        {
            HYP_LOG(Editor, Warning, "Using the previous build of game assembly {}: {}", modulePath, copyResult.GetError().GetMessage());

            return currentModule;
        }

        return copyResult.GetError();
    }

    const bool isReload = currentModule != nullptr;

    if (!isReload)
    {
        currentModule = GetLoadedManagedModules().PushBack(MakeUnique<LoadedManagedModule>()).Get();
    }

    // reloaded in place, so pointers to it stay valid
    LoadedManagedModule& loadedModule = *currentModule;
    loadedModule = LoadedManagedModule {};
    loadedModule.modulePath = modulePath;
    loadedModule.loadedTimestampOrError = timestamp;
    loadedModule.attemptedTimestamp = timestamp;

    const String gameClassName = GetNativeProjectName(projectFilepath) + "Game";

    LoadManagedModuleCopy(loadedModule, copyResult.GetValue(), gameClassName);

    if (const GameModuleError* error = loadedModule.GetLoadedError())
    {
        HYP_LOG(Editor, Error, "{}", error->GetMessage());

        return &loadedModule;
    }

    HYP_LOG(Editor, Info, "{} game assembly {} (Game class: {})", isReload ? "Reloaded" : "Loaded", modulePath, gameClassName);

    ScriptSystem::SetGameAssembly(projectFilepath.BasePath(), modulePath, /* isNewBuild */ true);

    if (isReload)
    {
        loadedModule.flags |= ManagedModuleFlags::NeedsRetarget;
    }

    return &loadedModule;
}

TResult<Handle<Game>> CreateManagedGame(const LoadedManagedModule& loadedModule)
{
    if (const GameModuleError* error = loadedModule.GetLoadedError())
    {
        return *error;
    }

    Handle<Game> game;

    {
        GlobalContextScope scope(ObjectInitializerContext { Game::StaticClass(), ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        game = MakeHandle<Game>();
    }

    ScriptObjectResource* scriptObjectResource = ScriptObjectFunctions::CreateScriptObjectResource_DotNet(game.Get(), loadedModule.gameClass);

    if (scriptObjectResource == nullptr || ScriptObjectFunctions::GetManagedObject(scriptObjectResource) == nullptr)
    {
        if (scriptObjectResource != nullptr)
        {
            ScriptObjectFunctions::DestroyScriptObjectResource(scriptObjectResource);
        }

        return HYP_MAKE_ERROR(Error, "Failed to create an instance of the Game class from {}", loadedModule.modulePath);
    }

    game->SetScriptObjectResource(scriptObjectResource);
    scriptObjectResource->AddReader();

    return game;
}

bool IsCurrentManagedGame(Game* game, const LoadedManagedModule& loadedModule)
{
    ScriptObjectResource* scriptObjectResource = game->GetScriptObjectResource();

    return scriptObjectResource != nullptr && scriptObjectResource->GetManagedClass() == loadedModule.gameClass;
}

// moves the project's objects and Game onto the latest build of the assembly
Result ApplyManagedModule(EditorProject* project, LoadedManagedModule& loadedModule)
{
    if (const GameModuleError* error = loadedModule.GetLoadedError())
    {
        return *error;
    }

    if (loadedModule.flags & ManagedModuleFlags::NeedsRetarget)
    {
        if (const Handle<World>& world = project->GetWorld(); world.IsValid())
        {
            ScriptSystem::RetargetReloadedObjects(*world);
        }

        loadedModule.flags &= ~ManagedModuleFlags::NeedsRetarget;
    }

    Handle<Game> oldGame = project->GetGame();

    if (oldGame.IsValid() && (IsCurrentManagedGame(oldGame.Get(), loadedModule) || oldGame->GetWorld().IsValid()))
    {
        return {};
    }

    TResult<Handle<Game>> gameResult = CreateManagedGame(loadedModule);

    if (gameResult.HasError())
    {
        return gameResult.GetError();
    }

    if (!oldGame.IsValid())
    {
        project->SetGame(gameResult.GetValue());

        return {};
    }

    ReplaceProjectGame(project, std::move(oldGame), gameResult.GetValue());

    return {};
}

} // namespace

#endif // HYP_DOTNET

Result ReloadManagedGameModule(EditorProject* project)
{
    const FilePath modulePath = GetManagedModulePath(project->GetFilePath());

    if (!modulePath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} isn't built", modulePath);
    }

#ifdef HYP_DOTNET
    if (!DotNETHost::GetInstance().IsInitialized())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} needs the .NET runtime", modulePath);
    }

    if (const Handle<Game>& game = project->GetGame(); game.IsValid() && game->GetWorld().IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module while the game is running");
    }

    LoadedManagedModule* loadedModule = nullptr;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(project->GetFilePath());

        if (moduleResult.HasError())
        {
            return moduleResult.GetError();
        }

        loadedModule = moduleResult.GetValue();
    }

    if (!(loadedModule->flags & ManagedModuleFlags::NeedsRetarget) && project->GetGame().IsValid() && IsCurrentManagedGame(project->GetGame().Get(), *loadedModule))
    {
        HYP_LOG(Editor, Info, "Game assembly {} is up to date", modulePath);

        return {};
    }

    Result result = ApplyManagedModule(project, *loadedModule);

    if (result.HasError())
    {
        loadedModule->flags |= ManagedModuleFlags::ApplyFailed;
    }
    else
    {
        loadedModule->flags &= ~ManagedModuleFlags::ApplyFailed;
    }

    return result;
#else
    return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} needs the .NET runtime", modulePath);
#endif
}

void UpdateManagedGameModule(EditorProject* project)
{
#ifdef HYP_DOTNET
    LoadedManagedModule* managedModule = nullptr;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        managedModule = FindGameModule(GetLoadedManagedModules(), GetManagedModulePath(project->GetFilePath()));

        if (managedModule != nullptr && PollModuleRebuilt(*managedModule))
        {
            TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(project->GetFilePath());

            managedModule = moduleResult.HasError() ? nullptr : moduleResult.GetValue();
        }
    }

    if (managedModule == nullptr
        || managedModule->GetLoadedError()
        || (managedModule->flags & ManagedModuleFlags::ApplyFailed))
    {
        return;
    }

    if (Result result = ApplyManagedModule(project, *managedModule); result.HasError())
    {
        HYP_LOG(Editor, Error, "{}", result.GetError().GetMessage());

        managedModule->flags |= ManagedModuleFlags::ApplyFailed;
    }
#endif
}

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
    return projectFilepath.BasePath() / "Binaries" / String(GetModulePlatformDirectory()) / (GetNativeProjectName(projectFilepath) + ".dll");
}

EDITOR_API Result WriteManagedProjectProps(const FilePath& projectFilepath, Span<const Name> pluginModules)
{
    const FilePath propsPath = projectFilepath.BasePath() / "Source" / "Hyperion.Local.props";

    const String binDir = String(CoreApi::GetExecutablePath()).ReplaceAll("\\", "/");
    const String baseDir = String(CoreApi::GetBaseDirectory()).ReplaceAll("\\", "/");

    String pluginModulesValue;

    for (Name moduleName : pluginModules)
    {
        if (pluginModulesValue.Any())
        {
            pluginModulesValue += "+";
        }

        pluginModulesValue += moduleName.LookupString();
    }

    const String text = String("<Project>\n  <PropertyGroup>\n")
        + "    <HyperionBinDir>" + binDir + "/</HyperionBinDir>\n"
        + "    <HyperionBaseDir>" + baseDir + "/</HyperionBaseDir>\n"
        + "    <HyperionSdkDir>" + baseDir + "/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
        + "    <HyperionPluginModules>" + pluginModulesValue + "</HyperionPluginModules>\n"
        + "  </PropertyGroup>\n</Project>\n";

    if (propsPath.Exists())
    {
        FileByteReader reader { propsPath };
        const ByteBuffer existing = reader.Read();
        reader.Close();

        if (String(existing.ToByteView()) == text)
        {
            return {};
        }
    }

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
    if (!IsManagedProject(projectFilepath)
        || !DotNETHost::GetInstance().IsInitialized())
    {
        return Handle<Game>();
    }

    const FilePath modulePath = GetManagedModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    LoadedManagedModule* loadedModule = nullptr;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(projectFilepath);

        if (moduleResult.HasError())
        {
            return moduleResult.GetError();
        }

        loadedModule = moduleResult.GetValue();
    }

    return CreateManagedGame(*loadedModule);
#else
    return Handle<Game>();
#endif
}

} // namespace Hyperion
