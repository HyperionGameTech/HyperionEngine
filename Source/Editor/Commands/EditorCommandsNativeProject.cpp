#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Editor/EditorNativeModule.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/Span.hpp>

#include <filesystem>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
} // namespace CoreApi

namespace PlatformUtils {
ENGINE_API extern bool OpenInFileBrowser(const FilePath& path);
} // namespace PlatformUtils

namespace /* Helpers */ {

struct TemplateFile
{
    FilePath templatePath;
    FilePath filepath;
};

Result WriteTemplateFile(const FilePath& templatePath, const FilePath& filepath, const String& name, const String& className)
{
    FileByteReader reader { templatePath };

    if (reader.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Template {} is missing", templatePath);
    }

    const ByteBuffer templateBuffer = reader.Read();
    reader.Close();

    const FilePath directory = filepath.BasePath();

    if (!directory.Exists() && !directory.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory {}", directory);
    }

    FileByteWriter writer { filepath };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", filepath);
    }

    const String text = String(templateBuffer.ToByteView()).ReplaceAll("@NAME@", name).ReplaceAll("@CLASS@", className);

    writer.Write(text.Data(), text.Size());
    writer.Close();

    return {};
}

} // namespace

#pragma region GenerateNativeProject

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
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid() || !currentProject->IsSaved())
        {
            bool cancel = false;

            SystemMessageBox(MessageBoxType::INFO)
                .Title("Save project?")
                .Text("The project must be saved before generating the C++ project, do you want to save?")
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
                .Button("Cancel", [&cancel]
                        {
                            cancel = true;
                        })
                .Show();

            if (cancel)
            {
                return;
            }
        }

        const FilePath sourceDir = currentProject->GetFilePath().BasePath() / "Source";

        if (sourceDir.Exists())
        {
            /// @TODO regenerate?
            return;
        }

        const String name = GetNativeProjectName(currentProject->GetFilePath());
        const String className = name + "Game";

        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/NativeGame";

        const TemplateFile files[] = {
            { templateDir / "CMakeLists.txt.in", sourceDir / "CMakeLists.txt" },
            { templateDir / "Game" / "Game.hpp.in", sourceDir / "Game" / (className + ".hpp") },
            { templateDir / "Game" / "Game.cpp.in", sourceDir / "Game" / (className + ".cpp") },
            { templateDir / "Launcher" / "main.cpp.in", sourceDir / "Launcher" / "main.cpp" }
        };

        for (const TemplateFile& file : files)
        {
            if (Result result = WriteTemplateFile(file.templatePath, file.filepath, name, className); result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to generate C++ project! {}", result.GetError().GetMessage());

                SystemMessageBox(MessageBoxType::CRITICAL)
                    .Title("Failed to generate C++ project!")
                    .Text(result.GetError().GetMessage())
                    .Button("OK", NoOpFunction<void> {})
                    .Show();

                return;
            }
        }

        HYP_LOG(Editor, Info, "Generated C++ project for '{}' at {}", name, sourceDir);

        PlatformUtils::OpenInFileBrowser(sourceDir);
    }
};

DEFINE_EDITOR_COMMAND(GenerateNativeProject);

#pragma endregion GenerateNativeProject

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
        g_editorState->OnBuildNativeGameRequested();
    }
};

DEFINE_EDITOR_COMMAND(BuildNativeGame);

#pragma endregion BuildNativeGame

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

// Game executable, engine libraries and config from the project's build output, set up to run from its own cooked content
Result StagePackagedGame(const FilePath& binariesDir, const FilePath& outputDir, const String& name, Name startupWorldName)
{
    static const char* const s_binaryExtensions[] = { ".exe", ".dll", ".so", ".dylib", "" };

    if (Result result = CopyFilesWithExtensions(binariesDir, outputDir, Span<const char* const>(s_binaryExtensions)); result.HasError())
    {
        return result;
    }

    // a redistributed editor carries the C++ runtime next to it; a dev build relies on the installed one
    static const char* const s_runtimeLibraries[] = {
        "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll",
        "vcruntime140.dll", "vcruntime140_1.dll", "vcruntime140_threads.dll", "concrt140.dll", "vccorlib140.dll"
    };

    for (const char* runtimeLibrary : s_runtimeLibraries)
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

    if ((binariesDir / "Config").Exists())
    {
        if (Result result = CopyFilesWithExtensions(binariesDir / "Config", configDir, {}); result.HasError())
        {
            return result;
        }
    }

    FileByteWriter writer { configDir / "GlobalConfig.json" };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to write {}", configDir / "GlobalConfig.json");
    }

    const String config = String("{\n  \"App\": {\n    \"Name\": \"") + name
        + "\",\n    \"Args\": \"--basedir=./ --singleplayer --startupworld=" + *startupWorldName
        + "\"\n  }\n}\n";

    writer.Write(config.Data(), config.Size());
    writer.Close();

    return {};
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

        const FilePath binariesDir = GetNativeModulePath(currentProject->GetFilePath()).BasePath();

        if (!GetNativeModulePath(currentProject->GetFilePath()).Exists())
        {
            ShowPackageGameMessage(MessageBoxType::INFO, "Build the game first", "Packaging uses the game built by Build Game.");

            return;
        }

        // [output folder] skips the dialog
        if (GetArguments().Any())
        {
            Package(currentProject, FilePath(GetArgument(0)));

            return;
        }

        ShowSelectFolderDialog(
            "Select a folder to package the game into",
            currentProject->GetFilePath().BasePath(),
            [currentProject](TResult<FilePath>&& result)
            {
                if (result.HasError() || result.GetValue().Empty())
                {
                    return;
                }

                Package(currentProject, result.GetValue());
            });
    }

private:
    static void Package(const Handle<EditorProject>& project, const FilePath& parentDir)
    {
        if (Result saveResult = project->Save(); saveResult.HasError())
        {
            ShowPackageGameMessage(MessageBoxType::CRITICAL, "Project could not be saved", saveResult.GetError().GetMessage());

            return;
        }

        const String name = GetNativeProjectName(project->GetFilePath());
        const FilePath outputDir = parentDir / name;

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
            "Cooking content",
            /* isForegroundTask */ true);

        TaskSystem::GetInstance().Enqueue(
            [editorTaskScope,
                name,
                outputDir,
                projectDir = project->GetFilePath().BasePath(),
                binariesDir = GetNativeModulePath(project->GetFilePath()).BasePath(),
                startupWorldName = project->GetEditWorldName()]()
            {
                CommandLineArguments args;
                args.Set("project", projectDir);
                args.Set("out-cache", outputDir / "Cache");
                args.Set("out-content", outputDir / "Content");
                // where a game looks for engine content relative to its base directory
                args.Set("out-engine-content", outputDir / "Content" / "Engine");

                Result result = g_appContext->RunCommandlet("BlobStorageCookCommandlet", args);

                if (!result.HasError())
                {
                    editorTaskScope->GetEditorTask()->SetDescription("Copying the game");

                    result = StagePackagedGame(binariesDir, outputDir, name, startupWorldName);
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
