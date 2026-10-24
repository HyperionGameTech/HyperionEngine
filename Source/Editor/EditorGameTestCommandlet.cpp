/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorState.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>

#include <Scene/World.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Logging/Logger.hpp>

#include <cstdlib>
#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

/// Runs a C++ game's test phases in its editor project without the editor, for Tests/GameProjects. Each build directory in turn is
/// copied into the project's Binaries and loaded as the editor loads a rebuilt game, then the game's test class runs that step's
/// phase. The results file lists each step, whether the replaced builds unloaded once the release phase ran, and whether the
/// project saved
class EditorGameTestCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(EditorGameTestCommandlet);

public:
    virtual ~EditorGameTestCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static const CommandLineArgumentDefinitions s_definitions = []()
        {
            CommandLineArgumentDefinitions definitions;

            definitions.Add("project", "", "The project directory", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
            definitions.Add("builds", "", "Comma-separated directories holding each build of the game module, in load order", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
            definitions.Add("phases", "", "Comma-separated phase to run on each build", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
            definitions.Add("release-phase", "", "Phase to run after the last build, before waiting for replaced builds to unload", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));
            definitions.Add("test-class", "", "The game class with a static RunPhase(String phase, Game game, World world) method", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
            definitions.Add("results", "", "File to write the results to", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
            definitions.Add("save-project", "", "Save the project as the editor does once the phases have run", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, JSON::Value(false));
            definitions.Add("rebuild-script", "", "Script to run where the editor would rebuild the game for this engine", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));

            return definitions;
        }();

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const FilePath projectFilepath = FindProjectFile(FilePath(args["project"].ToString()));
        const Array<String> buildDirectories = args["builds"].ToString().Split(',');
        const Array<String> phases = args["phases"].ToString().Split(',');
        const String releasePhase = args["release-phase"].ToString();
        const String testClassName = args["test-class"].ToString();
        const FilePath resultsPath = FilePath(args["results"].ToString());
        const bool saveProject = args["save-project"].ToBool();

        if (!projectFilepath.Exists())
        {
            return HYP_MAKE_ERROR(Error, "No project file in {}", args["project"].ToString());
        }

        if (buildDirectories.Size() != phases.Size())
        {
            return HYP_MAKE_ERROR(Error, "{} build(s) were given, but {} phase(s)", buildDirectories.Size(), phases.Size());
        }

        const FilePath binariesDirectory = GetNativeModulePath(projectFilepath).BasePath();

        DelegateHandler rebuildHandler;

        if (const String rebuildScript = args["rebuild-script"].ToString(); rebuildScript.Any())
        {
            rebuildHandler = g_editorState->OnRebuildNativeGameRequested.Bind(nullptr, [rebuildScript](const String&)
                {
                    HYP_LOG(Editor, Info, "Rebuilding the game with {}", rebuildScript);

                    return std::system(HYP_FORMAT("\"{}\"", rebuildScript).Data()) == 0;
                });
        }

        JSON::JArray steps;
        Handle<EditorProject> project;

        for (size_t index = 0; index < buildDirectories.Size(); index++)
        {
            JSON::Object step {
                { "build", JSON::Value(buildDirectories[index]) },
                { "phase", JSON::Value(phases[index]) } };

            Result loadResult = InstallBuild(FilePath(buildDirectories[index]), binariesDirectory);

            if (!loadResult.HasError())
            {
                loadResult = project.IsValid() ? ReloadGameModule(project.Get()) : OpenProject(projectFilepath, project);
            }

            if (project.IsValid())
            {
                UpdateGameModules(project.Get(), /* isSimulating */ false);
            }

            step.Set("loaded", JSON::Value(!loadResult.HasError()));

            if (loadResult.HasError())
            {
                HYP_LOG(Editor, Info, "Build {} didn't load: {}", buildDirectories[index], loadResult.GetError().GetMessage());

                step.Set("loadError", JSON::Value(String(loadResult.GetError().GetMessage())));
            }
            else
            {
                step.Set("report", RunPhase(project, testClassName, phases[index]));
            }

            steps.PushBack(JSON::Value(std::move(step)));

            if (!project.IsValid())
            {
                break;
            }
        }

        JSON::Object results;
        results.Set("steps", JSON::Value(std::move(steps)));

        if (project.IsValid())
        {
            results.Set("retiredBeforeRelease", JSON::Value(HasRetiredNativeModules()));

            if (releasePhase.Any())
            {
                results.Set("release", RunPhase(project, testClassName, releasePhase));
            }

            results.Set("unloaded", JSON::Value(WaitForRetiredModulesToUnload(project.Get())));

            if (saveProject)
            {
                const Result saveResult = project->Save();

                results.Set("saved", JSON::Value(!saveResult.HasError()));

                if (saveResult.HasError())
                {
                    results.Set("saveError", JSON::Value(String(saveResult.GetError().GetMessage())));
                }
            }

            project->Close();
        }

        FileByteWriter writer { resultsPath };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", resultsPath);
        }

        writer.WriteString(JSON::Value(std::move(results)).ToString(true).ToUtf8());
        writer.Close();

        return {};
    }

private:
    // each copy is newer than the build it replaces, as a rebuild would be
    static Result InstallBuild(const FilePath& buildDirectory, const FilePath& binariesDirectory)
    {
        std::error_code errorCode;

        std::filesystem::create_directories(binariesDirectory.Data(), errorCode);

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(buildDirectory.Data(), errorCode))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path target = std::filesystem::path(binariesDirectory.Data()) / entry.path().filename();

            if (!std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, errorCode))
            {
                return HYP_MAKE_ERROR(Error, "Failed to copy {} to {}: {}", entry.path().string().c_str(), binariesDirectory, errorCode.message().c_str());
            }

            std::filesystem::last_write_time(target, std::filesystem::file_time_type::clock::now(), errorCode);
        }

        if (errorCode)
        {
            return HYP_MAKE_ERROR(Error, "Failed to read build directory {}: {}", buildDirectory, errorCode.message().c_str());
        }

        // timestamps may only be as fine as a second
        ThreadSleep(1100);

        return {};
    }

    // loads the game module as the editor does when it opens the project, with a World to edit
    static Result OpenProject(const FilePath& projectFilepath, Handle<EditorProject>& outProject)
    {
        TResult<Handle<EditorProject>> loadResult = EditorProject::Load(projectFilepath);

        if (loadResult.HasError())
        {
            return loadResult.GetError();
        }

        Handle<EditorProject> project = std::move(loadResult.GetValue());
        InitObject(project);

        Handle<World> world = MakeHandle<World>(project->GetEditWorldName(), WorldFlags::Default);
        world->SetGame(project->GetGame());

        project->SetEditWorld(world);

        g_engineDriver->AddWorld(world);

        outProject = std::move(project);

        return {};
    }

    // a report of the phase, the same as the game writes, with one failed test
    static JSON::Value FailedPhase(const String& phase, const String& message)
    {
        JSON::JArray tests;

        tests.PushBack(JSON::Object {
            { "name", JSON::Value(String("RunPhase")) },
            { "passed", JSON::Value(false) },
            { "message", JSON::Value(message) },
            { "durationMs", JSON::Value(0.0) } });

        return JSON::Object {
            { "phase", JSON::Value(phase) },
            { "tests", JSON::Value(std::move(tests)) } };
    }

    static JSON::Value RunPhase(const Handle<EditorProject>& project, const String& testClassName, const String& phase)
    {
        const Class* testClass = ClassRegistry::GetInstance().GetClass(ANSIStringView(testClassName.Data()), /* ignoreCase */ false);
        const Method* runPhase = testClass != nullptr ? testClass->GetMethod("RunPhase"_sh) : nullptr;

        if (runPhase == nullptr)
        {
            return FailedPhase(phase, HYP_FORMAT("{} has no RunPhase method in the loaded build", testClassName));
        }

        BoxedValue phaseArgument { phase };
        BoxedValue gameArgument { project->GetGame() };
        BoxedValue worldArgument { project->GetWorld() };

        BoxedValue* arguments[] = { &phaseArgument, &gameArgument, &worldArgument };

        BoxedValue result = runPhase->Invoke(Span<BoxedValue*>(arguments, std::size(arguments)));

        if (!result.Is<String>())
        {
            return FailedPhase(phase, HYP_FORMAT("{}.RunPhase didn't return a String", testClassName));
        }

        JSON::ParseResult parseResult = JSON::Parse(result.Get<String>());

        if (!parseResult.ok)
        {
            return FailedPhase(phase, HYP_FORMAT("{}.RunPhase returned invalid JSON: {}", testClassName, parseResult.message));
        }

        return parseResult.value;
    }

    static bool WaitForRetiredModulesToUnload(EditorProject* project)
    {
        static constexpr int64 UnloadTimeoutMs = 40000;

        const Time startTime = Time::Now();

        while (HasRetiredNativeModules() && int64(Time::Now() - startTime) < UnloadTimeoutMs)
        {
            UpdateGameModules(project, /* isSimulating */ false);

            ThreadSleep(100);
        }

        return !HasRetiredNativeModules();
    }
};

HYP_EXPORT const Class* g_clsEditorGameTestCommandlet = nullptr;

const Class* EditorGameTestCommandlet::StaticClass()
{
    return g_clsEditorGameTestCommandlet;
}

HYP_BEGIN_CLASS(EditorGameTestCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "editorgametest"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(EditorGameTestCommandlet);

} // namespace Hyperion
