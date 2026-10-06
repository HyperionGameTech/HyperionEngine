#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <Rendering/ThumbnailCaptureState.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>

#include <Framework/View.hpp>
#include <Framework/EngineStats.hpp>

#include <Util/Img/WritePng.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Scene/EnvironmentSettings.hpp>

#include <Core/Utilities/Float16.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Semaphore.hpp>
#include <Core/Utilities/Time.hpp>

namespace Hyperion {

TResult<bool> SaveProjectAsWithPrompt(EditorProject* project)
{
    FilePath dir = EngineGlobals::GetProjectsDirectory();
    String currentFileName;

    if (project->IsSaved())
    {
        dir = project->GetFilePath().BasePath().BasePath();
        currentFileName = String(project->GetFilePath().Basename());
    }

    dir.IsDirectory() || dir.MkDir();

    FilePath selectedPath;
    Semaphore<int32> semaphore;

    ShowSaveFileDialog(
        "Save project as",
        dir,
        { "hypproject" },
        [&selectedPath, &semaphore](TResult<FilePath>&& result)
        {
            if (result.HasValue())
            {
                selectedPath = result.GetValue();
            }
            else
            {
                HYP_LOG(Editor, Info, "No project save path selected: {}", result.GetError().GetMessage());
            }

            semaphore.Produce();
        });

    // the dialog can return before a path has been picked
    semaphore.Acquire();

    if (selectedPath.Empty())
    {
        return false;
    }

    // the file name picked is the new project name
    const String projectName = FilePath(selectedPath.Basename()).StripExtension();
    if (projectName.Empty())
    {
        HYP_LOG(Editor, Warning, "No project name given.");
        return false;
    }

    const FilePath selectedDir = selectedPath.BasePath();

    const bool isProjectDir = String(selectedDir.Basename()) == projectName
        || (currentFileName.Any() && (selectedDir / currentFileName).Exists());

    selectedPath = (isProjectDir ? selectedDir : selectedDir / projectName) / (projectName + ".hypproject");

    if (Result saveResult = project->SaveAs(selectedPath); saveResult.HasError())
    {
        return saveResult.GetError();
    }

    return true;
}

TResult<bool> SaveProjectWithPrompt(EditorProject* project)
{
    if (!project->IsSaved())
    {
        return SaveProjectAsWithPrompt(project);
    }

    if (Result saveResult = project->Save(); saveResult.HasError())
    {
        return saveResult.GetError();
    }

    return true;
}

bool SaveProjectWithPromptOrAlert(EditorProject* project, const String& abortText)
{
    TResult<bool> saveResult = SaveProjectWithPrompt(project);

    if (saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

        SystemMessageBox(MessageBoxType::CRITICAL)
            .Title("Project could not be saved")
            .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage() + "\n" + abortText)
            .Button("OK", NoOpFunction<void> {})
            .Show();

        return false;
    }

    return saveResult.GetValue();
}

namespace /* Helpers */ {

bool ConfirmCloseCurrentProject(EditorSubsystem* subsystem)
{
    Handle<EditorProject> currentProject = subsystem->GetCurrentProject();

    if (!currentProject.IsValid() || !currentProject->IsDirty())
    {
        return true;
    }

    bool cancel = false;
    bool shouldSave = false;

    SystemMessageBox(MessageBoxType::INFO)
        .Title("Save changes?")
        .Text("Closing this project will discard any unsaved changes. Do you want to save changes before exiting?")
        .Button("Save", [&shouldSave]
                {
                    shouldSave = true;
                })
        .Button("Discard", NoOpFunction<void> {})
        .Button("Cancel", [&cancel]
                {
                    cancel = true;
                })
        .Show();

    if (cancel)
    {
        return false;
    }

    return !shouldSave || SaveProjectWithPromptOrAlert(currentProject, "The operation will be aborted to prevent loss of data");
}

void ParseWorldCommandArguments(const EditorCommandBase& command, Name& outWorldName, bool& outSaveWithoutAsking)
{
    outWorldName = Name::Invalid();
    outSaveWithoutAsking = false;

    for (const String& argument : command.GetArguments())
    {
        if (argument == "--save")
        {
            outSaveWithoutAsking = true;
        }
        else if (!argument.StartsWith("--") && !outWorldName.IsValid())
        {
            outWorldName = CreateNameFromDynamicString(ANSIString(argument));
        }
    }
}

// Leaving a World drops it from memory, so it has to be on disk first
bool EnsureProjectSavedBeforeWorldSwitch(EditorSubsystem* subsystem, bool saveWithoutAsking)
{
    Handle<EditorProject> currentProject = subsystem->GetCurrentProject();

    if (!currentProject.IsValid())
    {
        return false;
    }

    if (currentProject->IsSaved() && !currentProject->IsDirty())
    {
        return true;
    }

    bool shouldSave = saveWithoutAsking;

    if (!shouldSave)
    {
        SystemMessageBox(MessageBoxType::INFO)
            .Title("Save changes?")
            .Text("The project has to be saved before another World can be opened for editing.")
            .Button("Save", [&shouldSave]
                    {
                        shouldSave = true;
                    })
            .Button("Cancel", NoOpFunction<void> {})
            .Show();
    }

    if (!shouldSave)
    {
        return false;
    }

    return SaveProjectWithPromptOrAlert(currentProject, "The World will not be switched to prevent loss of data");
}

} // namespace

#pragma region Undo

class EditorCommandUndo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandUndo);

public:
    virtual ~EditorCommandUndo() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (EditorActionStack* actionStack = subsystem->GetActiveActionStack())
        {
            actionStack->Undo();
        }
    }
};

DEFINE_EDITOR_COMMAND(Undo);

#pragma endregion Undo

#pragma region Redo

class EditorCommandRedo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRedo);

public:
    virtual ~EditorCommandRedo() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (EditorActionStack* actionStack = subsystem->GetActiveActionStack())
        {
            actionStack->Redo();
        }
    }
};

DEFINE_EDITOR_COMMAND(Redo);

#pragma endregion Redo

#pragma region NewProject

class EditorCommandNewProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewProject);

public:
    virtual ~EditorCommandNewProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (!ConfirmCloseCurrentProject(subsystem))
        {
            return;
        }

        subsystem->NewProject();
    }
};

DEFINE_EDITOR_COMMAND(NewProject);

#pragma endregion NewProject

#pragma region OpenProject

class EditorCommandOpenProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandOpenProject);

public:
    virtual ~EditorCommandOpenProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (!ConfirmCloseCurrentProject(subsystem))
        {
            return;
        }

        const FilePath& dir = EngineGlobals::GetProjectsDirectory();
        dir.IsDirectory() || dir.MkDir();

        ShowOpenFileDialog(
            "Select the project to open",
            dir,
            { "hypproject" },
            /* allowMultiple */ false, /* allowDirectories */ true,
            [weakSubsystem = MakeWeakRef(subsystem)](TResult<Array<FilePath>>&& result) mutable
            {
                if (result.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to select project file: {}", result.GetError().GetMessage());
                    return;
                }

                if (result->Empty())
                {
                    HYP_LOG(Editor, Warning, "No project file selected.");
                    return;
                }

                HYP_LOG(Editor, Info, "Selected dir for open: {}", result.GetValue()[0]);

                // clang-format off
                GetThreadById(g_simThread)->GetScheduler().Enqueue(
                    [weakSubsystem = std::move(weakSubsystem), projectFilepath = std::move(result.GetValue()[0])]() mutable
                    {
                        Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                        if (!subsystem)
                        {
                            HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowOpenProjectDialog");
                            return;
                        }

                        subsystem->CloseProject();

                        TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectFilepath);

                        if (loadProjectResult.HasError())
                        {
                            HYP_LOG(Editor, Error, "Failed to load project: {}", loadProjectResult.GetError().GetMessage());
                            return;
                        }

                        Handle<EditorProject> project = loadProjectResult.GetValue();

                        if (!project.IsValid())
                        {
                            HYP_LOG(Editor, Error, "Loaded project is invalid.");
                            return;
                        }

                        subsystem->OpenProject(project);
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
                // clang-format on
            });
    }
};

DEFINE_EDITOR_COMMAND(OpenProject);

#pragma endregion OpenProject

#pragma region OpenProjectAtPath

class EditorCommandOpenProjectAtPath final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandOpenProjectAtPath);

public:
    virtual ~EditorCommandOpenProjectAtPath() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const FilePath projectFilepath = FilePath(GetArgument(0));

        if (projectFilepath.Empty())
        {
            HYP_LOG(Editor, Warning, "OpenProjectAtPath: no project path given");
            return;
        }

        if (!projectFilepath.Exists())
        {
            HYP_LOG(Editor, Error, "Project file does not exist: {}", projectFilepath);

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Project not found")
                .Text(String("The project could not be found at:\n") + projectFilepath + "\n\nIt has been removed from the recent projects list.")
                .Button("OK", NoOpFunction<void> {})
                .Show();

            g_editorState->RemoveRecentProject(projectFilepath);

            return;
        }

        if (Handle<EditorProject> currentProject = subsystem->GetCurrentProject(); currentProject.IsValid() && currentProject->GetFilePath() == projectFilepath)
        {
            return;
        }

        if (!ConfirmCloseCurrentProject(subsystem))
        {
            return;
        }

        subsystem->CloseProject();

        TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectFilepath);

        if (loadProjectResult.HasError() || !loadProjectResult.GetValue().IsValid())
        {
            const String errorMessage = loadProjectResult.HasError() ? String(loadProjectResult.GetError().GetMessage()) : String("Loaded project is invalid.");

            HYP_LOG(Editor, Error, "Failed to load project '{}': {}", projectFilepath, errorMessage);

            SystemMessageBox(MessageBoxType::CRITICAL)
                .Title("Project could not be opened")
                .Text(String("The project could not be opened: ") + errorMessage)
                .Button("OK", NoOpFunction<void> {})
                .Show();

            // Don't leave the editor without a project
            subsystem->NewProject();

            return;
        }

        subsystem->OpenProject(loadProjectResult.GetValue());
    }
};

DEFINE_EDITOR_COMMAND(OpenProjectAtPath);

#pragma endregion OpenProjectAtPath

#pragma region SaveProject

class EditorCommandSaveProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveProject);

public:
    virtual ~EditorCommandSaveProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            TResult<bool> result = SaveProjectWithPrompt(project);
            if (result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to save project: {}", result.GetError().GetMessage());
            }
        }
    }
};

DEFINE_EDITOR_COMMAND(SaveProject);

#pragma endregion SaveProject

#pragma region SaveProjectAs

class EditorCommandSaveProjectAs final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveProjectAs);

public:
    virtual ~EditorCommandSaveProjectAs() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            if (NumArguments() >= 1 && !GetArgument(0).Empty())
            {
                Result saveResult = project->SaveAs(FilePath(GetArgument(0)));
                if (!saveResult)
                {
                    HYP_LOG(Editor, Error, "Failed to save project as '{}': {}", GetArgument(0), saveResult.GetError().GetMessage());
                }

                return;
            }

            TResult<bool> saveResult = SaveProjectAsWithPrompt(project);
            if (saveResult.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());
            }
        }
    }
};

DEFINE_EDITOR_COMMAND(SaveProjectAs);

#pragma endregion SaveProjectAs

#pragma region CloseProject

class EditorCommandCloseProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCloseProject);

public:
    virtual ~EditorCommandCloseProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (!ConfirmCloseCurrentProject(subsystem))
        {
            return;
        }

        subsystem->CloseProject();
    }
};

DEFINE_EDITOR_COMMAND(CloseProject);

#pragma endregion CloseProject

#pragma region NewWorld

class EditorCommandNewWorld final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewWorld);

public:
    virtual ~EditorCommandNewWorld() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Warning, "NewWorld: no World name given (usage: NewWorld <name>)");
            return;
        }

        subsystem->NewWorldAsset(CreateNameFromDynamicString(ANSIString(GetArgument(0))));
    }
};

DEFINE_EDITOR_COMMAND(NewWorld);

#pragma endregion NewWorld

#pragma region OpenWorld

class EditorCommandOpenWorld final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandOpenWorld);

public:
    virtual ~EditorCommandOpenWorld() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Name worldName;
        bool saveWithoutAsking = false;

        ParseWorldCommandArguments(*this, worldName, saveWithoutAsking);

        if (!worldName.IsValid())
        {
            HYP_LOG(Editor, Warning, "OpenWorld: no World name given (usage: OpenWorld <name> [--save])");
            return;
        }

        if (Handle<EditorProject> currentProject = subsystem->GetCurrentProject(); currentProject.IsValid())
        {
            if (const Handle<World>& currentWorld = currentProject->GetWorld(); currentWorld.IsValid() && currentWorld->GetName() == worldName)
            {
                return;
            }
        }

        if (!EnsureProjectSavedBeforeWorldSwitch(subsystem, saveWithoutAsking))
        {
            return;
        }

        subsystem->OpenWorld(worldName);
    }
};

DEFINE_EDITOR_COMMAND(OpenWorld);

#pragma endregion OpenWorld


#pragma region CookGameContent

class EditorCommandCookGameContent final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCookGameContent);

public:
    virtual ~EditorCommandCookGameContent() override = default;

    virtual String GetText() const override
    {
        return "Cook Game Content";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Assert(g_appContext.IsValid(), "invalid app context");

        // Cant use EngineGlobals::GetContentDirectory(), since if we're executing from
        // the editor, it will refer to the current projects dir
        FilePath outContentDir = CoreApi::GetExecutablePath() / "Content";
        FilePath outCacheDir = EngineGlobals::GetCacheDirectory();

        if (!outContentDir.Any() || !outCacheDir.Any())
        {
            HYP_LOG(Editor, Error, "Must provide both content and cache dirs");
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot cook game content");

            return;
        }

        if (currentProject->IsSaved())
        {
            // If its already been saved then save the project again first so assets are totally up to date
            Result saveResult = currentProject->Save();
            if (saveResult.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());
                return;
            }
        }
        else
        {
            // Not saved, alert the user that we need them to save the project before this:
            bool shouldSave = false;

            SystemMessageBox(MessageBoxType::INFO)
                .Title("Must be saved before cooking game content")
                .Text("The current project is not yet saved - would you like to save the project to continue with the cook task?")
                .Button("Save", [&shouldSave]
                        {
                            shouldSave = true;
                        })
                .Button("Cancel", NoOpFunction<void> {})
                .Show();

            if (!shouldSave || !SaveProjectWithPromptOrAlert(currentProject, "The operation will be aborted to prevent loss of data"))
            {
                return;
            }
        }

        EditorTaskScope* editorTaskScope = new EditorTaskScope(
            TickableEditorTask::StaticClass(),
            []()
            { /* no tick function */ },
            "Cooking Game Content",
            "Initializing cook task",
            /* isForegroundTask */ true);

        Result saveResult = currentProject->Save();
        if (saveResult.HasError())
        {
            HYP_LOG(Editor, Error, "Project could not be saved, backing out of cook. Error was: {}", saveResult.GetError().GetMessage());

            editorTaskScope->GetEditorTask()->SetDescription(saveResult.GetError().GetMessage());

            delete editorTaskScope;

            return;
        }

        TaskSystem::GetInstance().Enqueue(
            [editorTaskScope, projectDir = currentProject->GetFilePath().BasePath(), outCacheDir, outContentDir]()
            {
                CommandLineArguments args;
                args.Set("project", projectDir);
                args.Set("out-cache", outCacheDir);
                args.Set("out-content", outContentDir);

                Result cookResult = g_appContext->RunCommandlet("BlobStorageCookCommandlet", args);

                if (cookResult.HasError())
                {
                    editorTaskScope->GetEditorTask()->SetDescription(cookResult.GetError().GetMessage());

                    ThreadSleep(5000);
                }

                delete editorTaskScope;
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND,
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
};

DEFINE_EDITOR_COMMAND(CookGameContent);

#pragma endregion CookGameContent

#pragma region CaptureViewport

class EditorCommandCaptureViewport final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCaptureViewport);

public:
    virtual ~EditorCommandCaptureViewport() override = default;

    virtual String GetText() const override
    {
        return "Capture Viewport";
    }

    virtual bool AllowedWhileSimulating() const override
    {
        return true;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        EditorViewport* activeViewport = subsystem->GetActiveViewport();

        if (!activeViewport || !activeViewport->GetView().IsValid())
        {
            HYP_LOG(Editor, Warning, "no active viewport");
            return;
        }

        const FilePath path = NumArguments() >= 1 && !GetArgument(0).Empty()
            ? FilePath(GetArgument(0))
            : CoreApi::GetExecutablePath() / "Screenshots" / "Viewport.png";

        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            [view = activeViewport->GetView(), path]()
            {
                static constexpr uint64 CaptureTimeoutMs = 5000;
                static ThumbnailCaptureState* s_pendingCapture = nullptr;
                static uint64 s_pendingSinceMs = 0;

                if (view->thumbnailCaptureState != nullptr)
                {
                    if (view->thumbnailCaptureState != s_pendingCapture || Time::Now().ToMilliseconds() - s_pendingSinceMs < CaptureTimeoutMs)
                    {
                        HYP_LOG(Editor, Warning, "a capture of this viewport is already in flight");
                        return;
                    }

                    HYP_LOG(Editor, Warning, "abandoning a viewport capture that never completed");
                    view->thumbnailCaptureState = nullptr;
                }

                const FramebufferRef& framebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

                if (!framebuffer.IsValid())
                {
                    HYP_LOG(Editor, Warning, "the viewport hasn't rendered yet");
                    return;
                }

                ThumbnailCaptureState* captureState = new ThumbnailCaptureState(framebuffer->GetExtent());
                view->thumbnailCaptureState = captureState;

                s_pendingCapture = captureState;
                s_pendingSinceMs = Time::Now().ToMilliseconds();

                captureState->Request(
                    [view, path, captureState](ByteBuffer&& color, ByteBuffer&&, Vec2u extent)
                    {
                        const uint32 numPixels = extent.x * extent.y;

                        if (numPixels != 0 && color.Size() >= size_t(numPixels) * 8)
                        {
                            ByteBuffer rgba8(size_t(numPixels) * 4);

                            //shit for RGBA16F RT
                            const Float16* src = reinterpret_cast<const Float16*>(color.Data());
                            ubyte* dst = rgba8.Data();

                            for (uint32 i = 0; i < numPixels; i++)
                            {
                                // sRGB
                                for (uint32 c = 0; c < 3; c++)
                                {
                                    float value = MathUtil::Clamp(float(src[i * 4 + c]), 0.0f, 1.0f);

                                    value = value <= 0.0031308f
                                        ? value * 12.92f
                                        : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;

                                    dst[i * 4 + c] = ubyte(MathUtil::Clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
                                }

                                dst[i * 4 + 3] = 255;
                            }

                            path.BasePath().MkDir();

                            if (WritePng::Write(path, extent.x, extent.y, 4, rgba8.Data()))
                            {
                                HYP_LOG(Editor, Info, "EditorCommandCaptureViewport: wrote {} ({}x{})", path, extent.x, extent.y);
                            }
                            else
                            {
                                HYP_LOG(Editor, Error, "EditorCommandCaptureViewport: failed to write {}", path);
                            }
                        }

                        if (view->thumbnailCaptureState == captureState)
                        {
                            view->thumbnailCaptureState = nullptr;
                        }

                        // not from inside its own callback
                        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
                            [captureState]()
                            {
                                delete captureState;
                            },
                            TaskEnqueueFlags::FIRE_AND_FORGET);
                    });
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
};

DEFINE_EDITOR_COMMAND(CaptureViewport);

#pragma endregion CaptureViewport

#pragma region SetEnvironment

// SetEnvironment {"Exposure": {"Contrast": 1.2}} to deep merge into the open World's EnvironmentSettings
class EditorCommandSetEnvironment final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSetEnvironment);

public:
    virtual ~EditorCommandSetEnvironment() override = default;

    virtual String GetText() const override
    {
        return "Set Environment";
    }

    virtual bool AllowedWhileSimulating() const override
    {
        return true;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid() || !currentProject->GetWorld().IsValid())
        {
            HYP_LOG(Editor, Warning, "SetEnvironment: no World open");
            return;
        }

        const Handle<World>& world = currentProject->GetWorld();
        const Class* environmentClass = GetClass<EnvironmentSettings>();

        EnvironmentSettings environmentSettings = world->GetEnvironmentSettings();
        BoxedValue target = BoxedValue(AnyRef(environmentClass->GetTypeInfo(), &environmentSettings));

        JSON::Object environmentJson;

        if (!ObjectToJSON(environmentClass, target, environmentJson))
        {
            HYP_LOG(Editor, Error, "SetEnvironment: could not serialize EnvironmentSettings");
            return;
        }

        if (NumArguments() != 0)
        {
            String changesText;

            for (const String& argument : GetArguments())
            {
                changesText = changesText.Empty() ? argument : changesText + " " + argument;
            }

            const JSON::ParseResult parseResult = JSON::Parse(changesText);

            if (!parseResult.ok || !parseResult.value.IsObject())
            {
                HYP_LOG(Editor, Error, "SetEnvironment: expected a JSON object, got {}", changesText);
                return;
            }

            environmentJson.MergeDeep(parseResult.value.AsObject());

            if (!ObjectFromJSON(environmentJson, environmentClass, target))
            {
                HYP_LOG(Editor, Error, "SetEnvironment: could not apply {}", changesText);
                return;
            }

            world->SetEnvironmentSettings(environmentSettings);
            world->MarkDirty();

            environmentJson = JSON::Object();
            ObjectToJSON(environmentClass, target, environmentJson);
        }

        HYP_LOG(Editor, Info, "SetEnvironment: {}", JSON::Value(environmentJson).ToString());
    }
};

DEFINE_EDITOR_COMMAND(SetEnvironment);

#pragma endregion SetEnvironment

#pragma region LogStats

class EditorCommandLogStats final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandLogStats);

public:
    virtual ~EditorCommandLogStats() override = default;

    virtual String GetText() const override
    {
        return "Log Stats";
    }

    virtual bool AllowedWhileSimulating() const override
    {
        return true;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EngineStats>& engineStats = EngineStats::GetInstance();

        if (!engineStats.IsValid() || NumArguments() < 1)
        {
            HYP_LOG(Editor, Warning, "LogStats: expected a stat or group path, e.g. Rendering/GPU/Glimmer");
            return;
        }

        const String path = GetArgument(0);

        EngineStatBase* stat = engineStats->GetStat(ANSIStringView(path.Data()));

        if (!stat)
        {
            HYP_LOG(Editor, Warning, "LogStats: no stat at {}", path);
            return;
        }

        const auto logStat = [&engineStats](EngineStatBase* statToLog)
        {
            const EngineStatsSnapshotValue& value = engineStats->GetCurrentSnapshot()[*statToLog];

            HYP_LOG(Editor, Info, "LogStats {}: value={} avg={} min={} max={}", statToLog->name, value.value, value.avg, value.min, value.max);
        };

        if (stat->type != EST_GROUP)
        {
            logStat(stat);
            return;
        }

        for (EngineStatBase* child : static_cast<EngineStatGroup*>(stat)->stats)
        {
            if (child->type != EST_GROUP)
            {
                logStat(child);
            }
        }
    }
};

DEFINE_EDITOR_COMMAND(LogStats);

#pragma endregion LogStats

} // namespace Hyperion
