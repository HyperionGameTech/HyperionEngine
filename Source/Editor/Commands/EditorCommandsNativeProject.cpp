#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProjectLayout.hpp>
/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Editor/EditorProjectSettings.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/Span.hpp>

#include <ctime>
#include <filesystem>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
} // namespace CoreApi

namespace PlatformUtils {
ENGINE_API extern bool OpenInFileBrowser(const FilePath& path);
} // namespace PlatformUtils

#pragma region GenerateNativeProject

namespace /* Helpers */ {

void GenerateGameProject(EditorSubsystem* subsystem, GameProjectLanguage language, bool openFolder)
{
    const char* languageName = language == GameProjectLanguage::Managed ? "C#" : "C++";

    const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

    if (!currentProject.IsValid() || !currentProject->IsSaved())
    {
        if (!currentProject.IsValid())
        {
            return;
        }

        bool shouldSave = false;

        SystemMessageBox(MessageBoxType::INFO)
            .Title("Save project?")
            .Text(HYP_FORMAT("The project must be saved before generating the {} project, do you want to save?", languageName))
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

    const FilePath projectFilepath = currentProject->GetFilePath();
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";

    const GameProjectLanguage currentLanguage = GetGameProjectLanguage(projectFilepath);

    if (currentLanguage != GameProjectLanguage::None && (currentLanguage == language || !IsGameProjectUnmodified(projectFilepath)))
    {
        PlatformUtils::OpenInFileBrowser(sourceDir);

        return;
    }

    const String moduleName = currentProject->GetModuleName();

    // once the code exists, a new title no longer renames it
    if (currentProject->GetProjectSettings()->moduleName != moduleName)
    {
        currentProject->GetProjectSettings()->moduleName = moduleName;

        if (!SaveProjectWithPromptOrAlert(currentProject, "The operation will be aborted to prevent loss of data"))
        {
            return;
        }
    }

    Result result;

    if (currentLanguage != GameProjectLanguage::None)
    {
        result = RemoveGameProjectFiles(projectFilepath);
    }

    if (!result.HasError())
    {
        result = GenerateGameProjectFiles(projectFilepath, language);
    }

    if (result.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to generate {} project! {}", languageName, result.GetError().GetMessage());

        SystemMessageBox(MessageBoxType::CRITICAL)
            .Title(HYP_FORMAT("Failed to generate {} project!", languageName))
            .Text(result.GetError().GetMessage())
            .Button("OK", NoOpFunction<void> {})
            .Show();

        return;
    }

    HYP_LOG(Editor, Info, "Generated {} project for '{}' at {}", languageName, moduleName, sourceDir);

    if (openFolder)
    {
        PlatformUtils::OpenInFileBrowser(sourceDir);
    }
}

} // namespace

class EditorCommandGenerateNativeProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandGenerateNativeProject);

public:
    virtual ~EditorCommandGenerateNativeProject() override = default;

    virtual String GetText() const override
    {
        return "Generate C++ Project";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        GenerateGameProject(subsystem, GameProjectLanguage::Native, !(GetArguments().Any() && GetArgument(0) == "--no-open"));
    }
};

DEFINE_EDITOR_COMMAND(GenerateNativeProject);

#pragma endregion GenerateNativeProject

#pragma region GenerateManagedProject

class EditorCommandGenerateManagedProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandGenerateManagedProject);

public:
    virtual ~EditorCommandGenerateManagedProject() override = default;

    virtual String GetText() const override
    {
        return "Generate C# Project";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        GenerateGameProject(subsystem, GameProjectLanguage::Managed, !(GetArguments().Any() && GetArgument(0) == "--no-open"));
    }
};

DEFINE_EDITOR_COMMAND(GenerateManagedProject);

#pragma endregion GenerateManagedProject

#pragma region BuildNativeGame

class EditorCommandBuildNativeGame final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildNativeGame);

public:
    virtual ~EditorCommandBuildNativeGame() override = default;

    virtual String GetText() const override
    {
        return "Build Game";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (currentProject.IsValid() && currentProject->IsSaved() && IsManagedProject(currentProject->GetFilePath()))
        {
            if (Result result = WriteManagedProjectProps(currentProject->GetFilePath()); result.HasError())
            {
                HYP_LOG(Editor, Error, "{}", result.GetError().GetMessage());
            }
        }

        g_editorState->OnBuildNativeGameRequested();
    }
};

DEFINE_EDITOR_COMMAND(BuildNativeGame);

#pragma endregion BuildNativeGame

#pragma region ReloadGameModule

class EditorCommandReloadGameModule final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandReloadGameModule);

public:
    virtual ~EditorCommandReloadGameModule() override = default;

    virtual String GetText() const override
    {
        return "Reload Game";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (Result result = ReloadGameModule(subsystem); result.HasError())
        {
            HYP_LOG(Editor, Error, "{}", result.GetError().GetMessage());
        }
    }
};

DEFINE_EDITOR_COMMAND(ReloadGameModule);

#pragma endregion ReloadGameModule

#pragma region BuildWebGame

class EditorCommandBuildWebGame final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildWebGame);

public:
    virtual ~EditorCommandBuildWebGame() override = default;

    virtual String GetText() const override
    {
        return "Build for Web";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const bool run = GetArguments().Any() && GetArgument(0) == "--run";

        RequestWebBuild(subsystem, run);
    }

    static void RequestWebBuild(EditorSubsystem* subsystem, bool run)
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid() || !currentProject->IsSaved())
        {
            SystemMessageBox(MessageBoxType::INFO)
                .Title("Save the project first")
                .Text("The project needs to be saved before it can be built for the web.")
                .Button("OK", NoOpFunction<void> {})
                .Show();

            return;
        }

        // the build reads the project from disk
        if (Result saveResult = currentProject->Save(); saveResult.HasError())
        {
            HYP_LOG(Editor, Error, "Project could not be saved: {}", saveResult.GetError().GetMessage());

            return;
        }

        g_editorState->OnBuildWebGameRequested(run);
    }
};

DEFINE_EDITOR_COMMAND(BuildWebGame);

class EditorCommandRunWebGame final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRunWebGame);

public:
    virtual ~EditorCommandRunWebGame() override = default;

    virtual String GetText() const override
    {
        return "Run in Browser";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid() || !currentProject->IsSaved())
        {
            SystemMessageBox(MessageBoxType::INFO)
                .Title("Save the project first")
                .Text("The project needs to be saved and built for the web before it can run in a browser.")
                .Button("OK", NoOpFunction<void> {})
                .Show();

            return;
        }

        g_editorState->OnRunWebGameRequested(currentProject->IsDirty());
    }
};

DEFINE_EDITOR_COMMAND(RunWebGame);

#pragma endregion BuildWebGame

#pragma region PackageGame

namespace /* Helpers */ {

void ShowPackageGameMessage(MessageBoxType type, const String& title, const String& text)
{
    SystemMessageBox(type)
        .Title(title)
        .Text(text)
        .Button("OK", NoOpFunction<void> {})
        .Show();
}

Result CopyFilesWithExtensions(const FilePath& sourceDir, const FilePath& targetDir, Span<const char* const> extensions)
{
    std::error_code errorCode;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sourceDir.Data(), errorCode))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string extension = entry.path().extension().string();

        bool matches = extensions.Size() == 0;

        for (const char* allowedExtension : extensions)
        {
            matches |= (extension == allowedExtension);
        }

        if (!matches)
        {
            continue;
        }

        std::filesystem::copy_file(entry.path(), std::filesystem::path(targetDir.Data()) / entry.path().filename(), std::filesystem::copy_options::overwrite_existing, errorCode);

        if (errorCode)
        {
            return HYP_MAKE_ERROR(Error, "Failed to copy {} to {}: {}", entry.path().string().c_str(), targetDir, errorCode.message().c_str());
        }
    }

    return {};
}

bool HasProjectScripts(const FilePath& projectFilepath)
{
    const TResult<FilePath> contentDirResult = ResolveProjectContentDirectory(projectFilepath);

    if (contentDirResult.HasError())
    {
        return false;
    }

    const FilePath scriptsDir = contentDirResult.GetValue() / "Scripts";

    if (!scriptsDir.Exists())
    {
        return false;
    }

    std::error_code errorCode;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir.Data(), errorCode))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".cs")
        {
            return true;
        }
    }

    return false;
}

bool HasNativeGameCode(const FilePath& projectFilepath)
{
    return (projectFilepath.BasePath() / "Source" / "Game").Exists();
}

Result StagePackagedGame(
    const FilePath& binariesDir,
    const FilePath& outputDir,
    const String& title,
    const String& moduleName,
    Name startupWorldName,
    const EditorNetSettings& netSettings)
{
#ifdef HYP_WINDOWS
    static const char* const s_executableExtension = ".exe";
#else
    static const char* const s_executableExtension = "";
#endif

    // the launcher is built under the module name, the player sees the title
    const FilePath builtExecutable = outputDir / (moduleName + s_executableExtension);
    const String executableName = MakeFileNameFromTitle(title);

    if (executableName.Any() && executableName != moduleName && builtExecutable.Exists())
    {
        const FilePath titledExecutable = outputDir / (executableName + s_executableExtension);

        if (titledExecutable.Exists())
        {
            titledExecutable.Remove();
        }

        if (!builtExecutable.Rename(titledExecutable))
        {
            return HYP_MAKE_ERROR(Error, "Failed to rename {} to {}", builtExecutable, titledExecutable);
        }
    }

    /// @TODO Refactor this gubbins!
    constexpr const char* const RuntimeLibraries[] = {
        "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll",
        "vcruntime140.dll", "vcruntime140_1.dll", "vcruntime140_threads.dll", "concrt140.dll", "vccorlib140.dll"
    };

    for (const char* runtimeLibrary : RuntimeLibraries)
    {
        const FilePath source = CoreApi::GetExecutablePath() / runtimeLibrary;

        if (source.Exists())
        {
            std::error_code errorCode;
            std::filesystem::copy_file(source.Data(), (outputDir / runtimeLibrary).Data(), std::filesystem::copy_options::overwrite_existing, errorCode);
        }
    }

    const FilePath configDir = outputDir / "Config";

    if (!configDir.Exists() && !configDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory {}", configDir);
    }

    // a project with only scripts has no build output of its own, so it takes the editor's engine config
    const FilePath sourceConfigDir = (binariesDir / "Config").Exists() ? binariesDir / "Config" : CoreApi::GetExecutablePath() / "Config";

    if (sourceConfigDir.Exists())
    {
        if (Result result = CopyFilesWithExtensions(sourceConfigDir, configDir, {}); result.HasError())
        {
            return result;
        }
    }

    FileByteWriter writer { configDir / "GlobalConfig.json" };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to write {}", configDir / "GlobalConfig.json");
    }

    JSON::Object appConfig;
    appConfig.Set("Name", JSON::JString(title));

    JSON::Object config;
    config.Set("App", JSON::Value(std::move(appConfig)));

    writer.WriteString(JSON::Value(std::move(config)).ToString(true).ToUtf8());
    writer.Close();

    Array<String> args { "--basedir=./", String("--startupworld=") + *startupWorldName };
    args.Concat(netSettings.GetGameArgs());

    return WriteGameArgsFile(outputDir, args);
}

} // namespace

class EditorCommandPackageGame final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPackageGame);

public:
    virtual ~EditorCommandPackageGame() override = default;

    virtual String GetText() const override
    {
        return "Package Game";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid() || !currentProject->IsSaved())
        {
            ShowPackageGameMessage(MessageBoxType::INFO, "Save the project first", "The project needs to be saved before it can be packaged.");

            return;
        }

        const FilePath& projectFilepath = currentProject->GetFilePath();

        if (!IsManagedProject(projectFilepath) && !HasNativeGameCode(projectFilepath) && !HasProjectScripts(projectFilepath))
        {
            ShowPackageGameMessage(MessageBoxType::INFO, "Nothing to package", "The project has no C# game, C++ game or scripts.");

            return;
        }

        if (GetArguments().Any())
        {
            Package(currentProject, FilePath(GetArgument(0)));

            return;
        }

        char timestamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&now));

        String folderName = MakeFileNameFromTitle(currentProject->GetGameTitle());

        if (folderName.Empty())
        {
            folderName = "Build";
        }

        Package(currentProject, currentProject->GetFilePath().BasePath() / "Packaged" / (folderName + "_" + timestamp));
    }

private:
    static void Package(const Handle<EditorProject>& project, const FilePath& outputDir)
    {
        if (Result saveResult = project->Save(); saveResult.HasError())
        {
            ShowPackageGameMessage(MessageBoxType::CRITICAL, "Project could not be saved", saveResult.GetError().GetMessage());

            return;
        }

        const String title = project->GetGameTitle();
        const String moduleName = GetNativeProjectName(project->GetFilePath());

        if (!outputDir.Exists() && !outputDir.MkDir())
        {
            ShowPackageGameMessage(MessageBoxType::CRITICAL, "Packaging failed", String("Could not create ") + outputDir);

            return;
        }

        EditorTaskScope* editorTaskScope = new EditorTaskScope(
            TickableEditorTask::StaticClass(),
            []()
            { /* no tick function */ },
            "Packaging Game",
            "Cooking and building the game",
            /* isForegroundTask */ true);

        TaskSystem::GetInstance().Enqueue(
            [editorTaskScope,
                title,
                moduleName,
                outputDir,
                projectFilepath = project->GetFilePath(),
                binariesDir = GetNativeModulePath(project->GetFilePath()).BasePath(),
                startupWorldName = project->GetEditWorldName(),
                netSettings = project->GetProjectSettings()->netSettings]()
            {
                Result result;

                if (!g_editorState->OnBuildPackageRequested(
                        projectFilepath,
                        binariesDir,
                        outputDir))
                {
                    result = HYP_MAKE_ERROR(Error, "Cooking or building the game failed, see the GameBuild log");
                }

                if (!result.HasError())
                {
                    editorTaskScope->GetEditorTask()->SetDescription("Copying the game");

                    result = StagePackagedGame(
                        binariesDir,
                        outputDir,
                        title,
                        moduleName,
                        startupWorldName,
                        netSettings);
                }

                if (result.HasError())
                {
                    HYP_LOG(Editor, Error, "Packaging failed: {}", result.GetError().GetMessage());

                    editorTaskScope->GetEditorTask()->SetDescription(result.GetError().GetMessage());

                    ThreadSleep(5000);
                }
                else
                {
                    HYP_LOG(Editor, Info, "Packaged game at {}", outputDir);

                    PlatformUtils::OpenInFileBrowser(outputDir);
                }

                delete editorTaskScope;
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND,
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
};

DEFINE_EDITOR_COMMAND(PackageGame);

#pragma endregion PackageGame


} // namespace Hyperion
