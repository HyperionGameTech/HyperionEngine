/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorGameModuleCommon.hpp>
#include <Editor/EditorProject.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Scene/World.hpp>

#include <Core/Utilities/Process.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Logging/Logger.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#endif

#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr int64 ModulePollIntervalMs = 1000;

static CVar<bool> s_cvAutoReloadGameModule { "Editor.AutoReloadGameModule", true };

Mutex& GetGameModulesMutex()
{
    static Mutex s_mutex;

    return s_mutex;
}

ANSIStringView GetModulePlatformDirectory()
{
#ifdef HYP_WINDOWS
    return "Windows";
#elif defined(HYP_MACOS)
    return "Darwin";
#else
    return "Linux";
#endif
}

static void RemoveCopiesOfExitedEditors(const FilePath& copyRoot)
{
    const uint32 currentProcess = GetCurrentProcessIdentifier();

    for (const FilePath& directory : copyRoot.GetSubdirectories())
    {
        uint32 processIdentifier = 0;

        if (!StringUtil::Parse(String(directory.Basename()), &processIdentifier)
            || processIdentifier == currentProcess
            || IsProcessRunning(processIdentifier))
        {
            continue;
        }

        if (!directory.RemoveRecursively())
        {
            HYP_LOG(Editor, Warning, "Failed to remove game module copies left by an exited editor in {}", directory);
        }
    }
}

TResult<FilePath> CopyModule(const FilePath& modulePath, ANSIStringView copyDirectoryName)
{
    const FilePath copyRoot = EngineGlobals::GetTempDirectory() / String(copyDirectoryName);
    const FilePath copyDirectory = copyRoot / HYP_FORMAT("{}", GetCurrentProcessIdentifier());

    if (!copyDirectory.Exists() && !copyDirectory.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create {} for the game module", copyDirectory);
    }

    RemoveCopiesOfExitedEditors(copyRoot);

    static uint32 s_copyCounter = 0;

    const String moduleStem = FilePath(modulePath.Basename()).StripExtension();

    const FilePath copyPath = copyDirectory / HYP_FORMAT("{}.{}.{}{}",
        moduleStem,
        uint64(Time::Now()),
        s_copyCounter++,
        modulePath.GetExtension().Any() ? String(".") + modulePath.GetExtension() : String());

    // FilePath has no copy
    std::error_code errorCode;
    std::filesystem::copy_file(modulePath.Data(), copyPath.Data(), std::filesystem::copy_options::overwrite_existing, errorCode);

    if (errorCode)
    {
        return HYP_MAKE_ERROR(Error, "Failed to copy game module {} to {}: {}", modulePath, copyPath, errorCode.message().c_str());
    }

    return copyPath;
}

bool PollModuleRebuilt(GameModuleBase& module)
{
    const Time now = Time::Now();

    if (!s_cvAutoReloadGameModule.Get() || int64(now - module.lastPollTime) < ModulePollIntervalMs)
    {
        return false;
    }

    module.lastPollTime = now;

    Time timestamp = Time(0);
    if (module.modulePath.Exists())
    {
        module.modulePath.LastModifiedTimestamp();
    }

    if (timestamp <= module.attemptedTimestamp)
    {
        return false;
    }

    if (timestamp == module.pendingTimestamp)
    {
        return true;
    }

    module.pendingTimestamp = timestamp;

    return false;
}

void CollectManagedGarbage()
{
#ifdef HYP_DOTNET
    if (TriggerGCFunction triggerGC = DotNETHost::GetInstance().GetGlobalFunctions().triggerGcFunction)
    {
        triggerGC();
    }
#endif
}

void ReplaceProjectGame(EditorProject* project, Handle<Game> oldGame, Handle<Game> newGame)
{
    newGame->SetIsServerGame(oldGame->IsServerGame());
    newGame->SetStartupWorldName(oldGame->GetStartupWorldName());
    newGame->SetAssetRegistry(oldGame->DetachAssetRegistry());

    if (const Handle<World>& world = project->GetWorld(); world.IsValid() && world->GetGame() == oldGame.Get())
    {
        world->SetGame(newGame.Get());
    }

    project->SetGame(newGame);

    oldGame->ReleaseManagedObject();
    oldGame.Reset();

    CollectManagedGarbage();
}

} // namespace Hyperion
