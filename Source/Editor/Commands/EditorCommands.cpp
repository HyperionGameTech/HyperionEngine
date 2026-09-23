#include <Editor/Commands/EditorCommandsCommon.hpp>

namespace Hyperion {

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
        if (Handle<EditorProject> currentProject = subsystem->GetCurrentProject(); currentProject.IsValid())
        {
            if (currentProject->IsDirty())
            {
                bool cancel = false;

                SystemMessageBox(MessageBoxType::INFO)
                    .Title("Save changes?")
                    .Text("Closing this project will discard any unsaved changes. Do you want to save changes before exiting?")
                    .Button("Save", [currentProject, &cancel]
                    {
                        Result saveResult = currentProject->Save();
                        if (saveResult.HasError())
                        {
                            HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                            SystemMessageBox(MessageBoxType::CRITICAL)
                                        .Title("Project could not be saved")
                                        .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                            + "\nThe operation will be aborted to prevent loss of data")
                                        .Button("OK", NoOpFunction<void> {})
                                        .Show();

                            cancel = true;
                        }
                    })
                    .Button("Discard", NoOpFunction<void> {})
                    .Button("Cancel", [&cancel] { cancel = true; })
                    .Show();

                if (cancel)
                {
                    return;
                }
            }
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
        if (Handle<EditorProject> currentProject = subsystem->GetCurrentProject(); currentProject.IsValid())
        {
            if (currentProject->IsDirty())
            {
                bool cancel = false;

                SystemMessageBox(MessageBoxType::INFO)
                    .Title("Save changes?")
                    .Text("Closing this project will discard any unsaved changes. Do you want to save changes before exiting?")
                    .Button("Save", [currentProject, &cancel]
                    {
                        Result saveResult = currentProject->Save();
                        if (saveResult.HasError())
                        {
                            HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                            SystemMessageBox(MessageBoxType::CRITICAL)
                                        .Title("Project could not be saved")
                                        .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                            + "\nThe operation will be aborted to prevent loss of data")
                                        .Button("OK", NoOpFunction<void> {})
                                        .Show();

                            cancel = true;
                        }
                    })
                    .Button("Discard", NoOpFunction<void> {})
                    .Button("Cancel", [&cancel] { cancel = true; })
                    .Show();

                if (cancel)
                {
                    return;
                }
            }
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

#pragma region SaveProject

class EditorCommandSaveProjectAs;

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
            if (!project->IsSaved())
            {
                Handle<EditorCommandSaveProjectAs> saveAs = MakeHandle<EditorCommandSaveProjectAs>();
                reinterpret_cast<EditorCommandBase&>(*saveAs).Execute(subsystem);

                return;
            }

            Result result = project->Save();
            if (!result)
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
            FilePath dir;

            if (project->IsSaved())
            {
                dir = project->GetFilePath().BasePath();
            }
            else
            {
                dir = EngineGlobals::GetProjectsDirectory() / *project->GetName();
            }

            dir.MkDir();

            String projectName = *project->GetName();

            ShowSelectFolderDialog(
                "Select project folder",
                dir,
                [weakSubsystem = MakeWeakRef(subsystem), projectName = std::move(projectName)](TResult<FilePath>&& result) mutable
                {
                    if (result.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to select project directory: {}", result.GetError().GetMessage());
                        return;
                    }

                    FilePath selectedPath = result.GetValue();
                    if (selectedPath.Empty())
                    {
                        HYP_LOG(Editor, Warning, "No save path selected.");
                        return;
                    }

                    if (selectedPath.EndsWith(projectName))
                    {
                        // IF the path we receive ends with the project name (ie. Projects/Project1) we want to chop off that part,
                        // otherwise we'd end up saving at Projects/Project1/Project1.
                        selectedPath = selectedPath.BasePath();
                    }

                    GetThreadById(g_simThread)->GetScheduler().Enqueue(
                        [weakSubsystem = std::move(weakSubsystem), selectedPath = std::move(selectedPath)]() mutable
                        {
                            Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                            if (!subsystem)
                            {
                                HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowSaveProjectDialog");
                                return;
                            }

                            EditorProject* project = subsystem->GetCurrentProject();
                            if (!project)
                            {
                                HYP_LOG(Editor, Error, "No current project in EditorSubsystem; cannot save project as.");
                                return;
                            }

                            Result saveResult = project->SaveAs(selectedPath);
                            if (!saveResult)
                            {
                                HYP_LOG(Editor, Error, "Failed to save project as '{}': {}", selectedPath, saveResult.GetError().GetMessage());
                            }
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                });
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
        if (Handle<EditorProject> currentProject = subsystem->GetCurrentProject(); currentProject.IsValid())
        {
            if (currentProject->IsDirty())
            {
                bool cancel = false;

                SystemMessageBox(MessageBoxType::INFO)
                    .Title("Save changes?")
                    .Text("Closing this project will discard any unsaved changes. Do you want to save changes before exiting?")
                    .Button("Save", [currentProject, &cancel]
                    {
                        Result saveResult = currentProject->Save();
                        if (saveResult.HasError())
                        {
                            HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                            SystemMessageBox(MessageBoxType::CRITICAL)
                                        .Title("Project could not be saved")
                                        .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                            + "\nThe operation will be aborted to prevent loss of data")
                                        .Button("OK", NoOpFunction<void> {})
                                        .Show();

                            cancel = true;
                        }
                    })
                    .Button("Discard", NoOpFunction<void> {})
                    .Button("Cancel", [&cancel] { cancel = true; })
                    .Show();

                if (cancel)
                {
                    return;
                }
            }
        }

        subsystem->CloseProject();
    }
};

DEFINE_EDITOR_COMMAND(CloseProject);

#pragma endregion CloseProject


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
            bool cancel = false;
            Result saveResult;

            SystemMessageBox(MessageBoxType::INFO)
                .Title("Must be saved before cooking game content")
                .Text("The current project is not yet saved - would you like to save the project to continue with the cook task?")
                .Button("Save", [currentProject, &saveResult]
                {
                    saveResult = currentProject->Save();
                    if (saveResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                        SystemMessageBox(MessageBoxType::CRITICAL)
                                    .Title("Project could not be saved")
                                    .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                        + "\nThe operation will be aborted to prevent loss of data")
                                    .Button("OK", NoOpFunction<void> {})
                                    .Show();
                            
                    }
                })
                .Button("Discard", NoOpFunction<void> {})
                .Button("Cancel", [&cancel] { cancel = true; })
                .Show();

            if (saveResult.HasError())
            {
                return;
            }

            if (cancel)
            {
                return; // ok, intentional cancel
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

} // namespace Hyperion
