/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorGame.hpp>
#include <Editor/EditorHelpers.hpp>
#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorProjectSettings.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Framework/Threads/MainThread.hpp>

#include <Framework/Testing/EngineTest.hpp>

#include <Plugins/PluginManager.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/World.hpp>
#include <Scene/System.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Logging/Logger.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedClass.hpp>
#endif

#include <cstring>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

constexpr int64 LaunchTimeoutMs = 60000;
constexpr int64 PlayTimeoutMs = 120000;

/// intentionally leaked!
Handle<EditorGame>& s_editorGame = *new Handle<EditorGame>();

template <typename PredicateType>
bool UpdateUntil(PredicateType&& predicate, int64 timeoutMs)
{
    const Time startTime = Time::Now();

    while (!predicate())
    {
        if (int64(Time::Now() - startTime) > timeoutMs)
        {
            return false;
        }

        g_mainThreadInstance->Update();
    }

    return true;
}

EditorPlayNetMode ParsePlayNetMode(const char* modeName)
{
    if (std::strcmp(modeName, "Client") == 0)
    {
        return EditorPlayNetMode::Client;
    }

    if (std::strcmp(modeName, "DedicatedServer") == 0)
    {
        return EditorPlayNetMode::DedicatedServer;
    }

    return EditorPlayNetMode::Standalone;
}

const char* GetPlayNetStatusName(EditorPlayNetStatus status)
{
    switch (status)
    {
    case EditorPlayNetStatus::Connecting:
        return "Connecting";
    case EditorPlayNetStatus::Connected:
        return "Connected";
    case EditorPlayNetStatus::Failed:
        return "Failed";
    case EditorPlayNetStatus::Disconnected:
        return "Disconnected";
    case EditorPlayNetStatus::Hosting:
        return "Hosting";
    default:
        return "None";
    }
}

String GetGameClassName(Game* game)
{
#ifdef HYP_DOTNET
    if (ScriptObjectResource* scriptObjectResource = game->GetScriptObjectResource())
    {
        if (const SharedPtr<dotnet::ManagedClass> managedClass = scriptObjectResource->GetManagedClass())
        {
            return String(managedClass->GetName());
        }
    }
#endif

    return String(*game->InstanceClass()->GetName());
}

bool StartEngine()
{
    if (!g_engineDriver->StartThreads())
    {
        HYP_LOG(Editor, Error, "Failed to start the engine threads");

        return false;
    }

    return true;
}

bool WriteResults(const FilePath& resultsPath, JSON::Object&& results)
{
    FileByteWriter writer { resultsPath };

    if (!writer.IsOpen())
    {
        HYP_LOG(Editor, Error, "Failed to open {} for writing", resultsPath);

        return false;
    }

    writer.WriteString(JSON::Value(std::move(results)).ToString(true).ToUtf8());
    writer.Close();

    return true;
}

JSON::JArray ToJSONArray(Span<const Name> names)
{
    JSON::JArray values;

    for (Name name : names)
    {
        values.PushBack(JSON::Value(String(name.LookupString())));
    }

    return values;
}

JSON::Object SimulateProject(const FilePath& projectDirectory, int numFrames)
{
    JSON::Object results;

    if (!PluginManager::GetInstance().IsInitialized())
    {
        PluginManager::GetInstance().Initialize(PluginHostMode::Editor);
    }

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(FindProjectFile(projectDirectory));

    results.Set("loaded", JSON::Value(!loadResult.HasError()));

    if (loadResult.HasError())
    {
        results.Set("loadError", JSON::Value(String(loadResult.GetError().GetMessage())));

        return results;
    }

    Handle<EditorProject> project = std::move(loadResult.GetValue());
    InitObject(project);

    PluginManager::GetInstance().SetEnabledPlugins(project->GetEnabledPlugins());

    results.Set("enabledPlugins", JSON::Value(ToJSONArray(PluginManager::GetInstance().GetEnabledPlugins())));

    Handle<Game> game = project->GetGame();

    results.Set("gameClass", JSON::Value(GetGameClassName(game.Get())));

    game->SetStartupWorldName(project->GetEditWorldName());

    // the sim thread is running, so it launches the game on its edit World now
    g_engineDriver->SetGameInstance(game.Get());

    int simulatedFrames = 0;

    if (UpdateUntil([&game] { return game->IsLaunched(); }, LaunchTimeoutMs))
    {
        game->StartSimulating();

        while (simulatedFrames < numFrames)
        {
            g_mainThreadInstance->Update();

            if (game->GetGameState().mode != GameStateMode::SIMULATING)
            {
                break;
            }

            simulatedFrames++;
        }
    }

    results.Set("launched", JSON::Value(game->IsLaunched()));
    results.Set("simulatedFrames", JSON::Value(simulatedFrames));
    results.Set("gameTime", JSON::Value(double(game->GetGameState().gameTime)));

    Array<Name> worldSystems;

    if (const Handle<World>& world = game->GetWorld(); world.IsValid())
    {
        for (const Handle<SystemBase>& system : world->GetSystems())
        {
            worldSystems.PushBack(system->InstanceClass()->GetName());
        }
    }

    results.Set("worldSystems", JSON::Value(ToJSONArray(worldSystems)));

    game->StopSimulating();

    results.Set("stopped", JSON::Value(game->GetGameState().mode == GameStateMode::STOPPED));

    project->Close();

    return results;
}

} // namespace

} // namespace Hyperion

using namespace Hyperion;

extern "C" {

HYP_EXPORT int EditorTestHost_CreateProject(const char* projectFilepath, int isManaged, const char* disabledPlugins)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    if (!PluginManager::GetInstance().IsInitialized())
    {
        PluginManager::GetInstance().Initialize(PluginHostMode::Editor);
    }

    Handle<EditorProject> project = EditorProject::CreateNew();

    EditorHelpers::PopulateNewWorld(*project->GetWorld(), project->GetGame()->GetAssetRegistry(), NAME("MainScene"));

    if (Result saveResult = project->SaveAs(FilePath(projectFilepath)); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project to {}: {}", projectFilepath, saveResult.GetError().GetMessage());

        return 0;
    }

    project->GetProjectSettings()->moduleName = project->GetModuleName();

    for (const String& pluginName : String(disabledPlugins).Split(','))
    {
        if (pluginName.Any())
        {
            project->SetPluginEnabled(CreateNameFromDynamicString(pluginName.ToAnsi()), false);
        }
    }

    if (Result saveResult = project->Save(); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project {}: {}", projectFilepath, saveResult.GetError().GetMessage());

        return 0;
    }

    const GameProjectLanguage projectLanguage = isManaged
        ? GameProjectLanguage::Managed  // C#
        : GameProjectLanguage::Native;  // C++

    if (Result generateResult = GenerateGameProjectFiles(
            project->GetFilePath(),
            projectLanguage,
            project->GetEnabledPluginRuntimeModules());
        generateResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to generate the game project for {}: {}", projectFilepath, generateResult.GetError().GetMessage());

        return 0;
    }

    project->Close();

    return 1;
}

HYP_EXPORT int EditorTestHost_Simulate(const char* projectDirectory, int numFrames, const char* resultsPath)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    return WriteResults(FilePath(resultsPath), SimulateProject(FilePath(projectDirectory), numFrames)) ? 1 : 0;
}

// simulates each project in turn in one process, as the editor does when it opens one project after another
HYP_EXPORT int EditorTestHost_Switch(const char* projectDirectories, int numFrames, const char* resultsPath)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    JSON::JArray steps;

    for (const String& projectDirectory : String(projectDirectories).Split(';'))
    {
        if (projectDirectory.Any())
        {
            steps.PushBack(JSON::Value(SimulateProject(FilePath(projectDirectory), numFrames)));
        }
    }

    JSON::Object results;
    results.Set("steps", JSON::Value(std::move(steps)));

    return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
}

HYP_EXPORT int EditorTestHost_Play(const char* projectDirectory, int numFrames, const char* netModeName, const char* host, int port, int expectedClients, const char* resultsPath)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    JSON::Object results;

    if (!PluginManager::GetInstance().IsInitialized())
    {
        PluginManager::GetInstance().Initialize(PluginHostMode::Editor);
    }

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(FindProjectFile(FilePath(projectDirectory)));

    results.Set("loaded", JSON::Value(!loadResult.HasError()));

    if (loadResult.HasError())
    {
        results.Set("loadError", JSON::Value(String(loadResult.GetError().GetMessage())));

        return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
    }

    Handle<EditorProject> project = std::move(loadResult.GetValue());
    InitObject(project);

    const FilePath projectFilePath = project->GetFilePath();

    s_editorGame = MakeHandle<EditorGame>();
    InitObject(s_editorGame);

    g_engineDriver->SetGameInstance(s_editorGame.Get());

    const bool editorLaunched = UpdateUntil([] { return s_editorGame->IsLaunched(); }, LaunchTimeoutMs);

    results.Set("editorLaunched", JSON::Value(editorLaunched));

    if (!editorLaunched)
    {
        return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
    }

    EditorSubsystem* editorSubsystem = s_editorGame->GetEditorSubsystem();
    Assert(editorSubsystem != nullptr);

    editorSubsystem->OpenProject(project);

    // Play In Editor settings are normally saved to the user's EditorConfig, which this run must leave alone,
    // even when it is killed partway through or another run is going at the same time
    editorSubsystem->SetPersistPlayNetSettings(false);

    const EditorPlayNetMode netMode = ParsePlayNetMode(netModeName);

    editorSubsystem->SetPlayNetMode(netMode);
    editorSubsystem->SetPlayNetHost(String(host));

    // 0 leaves the configured port, for modes that don't use it
    if (port != 0)
    {
        editorSubsystem->SetPlayNetPort(uint32(port));
    }

    // the server counts connections to it, which is how the harness checks a client got through
    int clientConnects = 0;
    int clientDisconnects = 0;

    DelegateHandler onClientConnected = g_gameServer->GetNetServer().OnClientConnected.Bind([&clientConnects](const NetServerConnectionStateChangedData&)
        {
            clientConnects++;
        });

    DelegateHandler onClientDisconnected = g_gameServer->GetNetServer().OnClientDisconnected.Bind([&clientDisconnects](const NetServerConnectionStateChangedData&)
        {
            clientDisconnects++;
        });

    const bool started = editorSubsystem->StartSimulation();

    results.Set("started", JSON::Value(started));

    Game* simulatedGame = started ? editorSubsystem->GetCurrentProject()->GetGame() : nullptr;

    int simulatedFrames = 0;
    bool timedOut = false;

    if (started)
    {
        // The editor runs the project's game while it plays (EngineManager.InitializeGame()), so it's the one that updates
        g_engineDriver->SetGameInstance(simulatedGame);

        const Time playStartTime = Time::Now();

        // headless, the sim thread is the main thread, so each update is a frame
        while (true)
        {
            g_mainThreadInstance->Update();

            if (int64(Time::Now() - playStartTime) > PlayTimeoutMs)
            {
                timedOut = true;

                break;
            }

            if (!simulatedGame->IsLaunched())
            {
                continue;
            }

            // a client only simulates once it has connected to the server
            if (netMode == EditorPlayNetMode::Client)
            {
                const EditorPlayNetStatus status = editorSubsystem->GetPlayNetStatus();

                if (status == EditorPlayNetStatus::Failed)
                {
                    break;
                }

                if (status != EditorPlayNetStatus::Connected)
                {
                    continue;
                }
            }

            if (simulatedGame->GetGameState().mode != GameStateMode::SIMULATING)
            {
                break;
            }

            simulatedFrames++;

            // a server holds on until the expected clients have connected and disconnected again
            if (simulatedFrames >= numFrames && clientConnects >= expectedClients && clientDisconnects >= expectedClients)
            {
                break;
            }
        }

        results.Set("launched", JSON::Value(simulatedGame->IsLaunched()));
        results.Set("gameClass", JSON::Value(GetGameClassName(simulatedGame)));
        results.Set("simulatedFrames", JSON::Value(simulatedFrames));
        results.Set("gameTime", JSON::Value(double(simulatedGame->GetGameState().gameTime)));
        results.Set("serverGame", JSON::Value(simulatedGame->IsServerGame()));
    }
    else
    {
        results.Set("launched", JSON::Value(false));
        results.Set("gameClass", JSON::Value(String("")));
        results.Set("simulatedFrames", JSON::Value(0));
        results.Set("gameTime", JSON::Value(0.0));
        results.Set("serverGame", JSON::Value(false));
    }

    results.Set("timedOut", JSON::Value(timedOut));
    results.Set("netStatus", JSON::Value(String(GetPlayNetStatusName(editorSubsystem->GetPlayNetStatus()))));
    results.Set("hosting", JSON::Value(g_gameServer->GetNetServer().IsListening()));
    results.Set("clientConnects", JSON::Value(clientConnects));
    results.Set("clientDisconnects", JSON::Value(clientDisconnects));

    bool stopped = false;

    if (started)
    {
        stopped = editorSubsystem->StopSimulation() && !editorSubsystem->IsSimulating();

        // back to the editor's game once the simulation is gone (EngineManager.InitializeEditor())
        g_engineDriver->SetGameInstance(s_editorGame.Get());
    }

    results.Set("stopped", JSON::Value(stopped));
    results.Set("listeningAfterStop", JSON::Value(g_gameServer->GetNetServer().IsListening()));

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();

    results.Set("editProjectRestored", JSON::Value(currentProject.IsValid() && currentProject->GetFilePath() == projectFilePath));

    editorSubsystem->CloseProject();

    return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
}

HYP_EXPORT int EditorTestHost_RunEditorTests(const char* filter, int listOnly, const char* resultsPath)
{
    AssertOnThread(g_mainThread);

#ifdef HYP_TESTS
    if (!StartEngine())
    {
        return 0;
    }

    if (!PluginManager::GetInstance().IsInitialized())
    {
        PluginManager::GetInstance().Initialize(PluginHostMode::Editor);
    }

    if (listOnly)
    {
        PluginManager::GetInstance().PreloadPlugins(PluginManager::GetInstance().GetDefaultEnabledPlugins());

        return tests::EngineTestRunner::RunTests(tests::EngineTestHost::Editor, String(filter), true, String(resultsPath)).HasError() ? 0 : 1;
    }

    s_editorGame = MakeHandle<EditorGame>();
    InitObject(s_editorGame);

    g_engineDriver->SetGameInstance(s_editorGame.Get());

    if (!UpdateUntil([] { return s_editorGame->IsLaunched(); }, LaunchTimeoutMs))
    {
        HYP_LOG(Editor, Error, "The editor didn't launch");

        return 0;
    }

    EditorSubsystem* editorSubsystem = s_editorGame->GetEditorSubsystem();
    Assert(editorSubsystem != nullptr);

    editorSubsystem->NewProject();

    const Result result = tests::EngineTestRunner::RunTests(tests::EngineTestHost::Editor, String(filter), false, String(resultsPath));

    editorSubsystem->CloseProject();

    return result.HasError() ? 0 : 1;
#else
    HYP_LOG(Editor, Error, "Editor tests need a build with HYP_TESTS");

    return 0;
#endif
}

} // extern "C"
